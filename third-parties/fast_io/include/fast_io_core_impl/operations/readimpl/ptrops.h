#pragma once

/*
 * Public pointer-range input operations (`operations` namespace).
 *
 * `read_some`, `read_all`, and their byte/positioned variants accept a user
 * stream handle, invoke `input_stream_ref` exactly once, validate character
 * compatibility, and enter normalized primitive synthesis. They transfer into
 * existing memory ranges; `io::scan` is the separate higher-level operation
 * that interprets those characters as target values.
 */

namespace fast_io
{

namespace operations
{

// A stream-ref customization is an ownership boundary, not a cheap conversion that internal fallbacks may repeat.
// `decltype(auto)` preserves a customization that deliberately returns an lvalue reference and materializes exactly
// one local owner when it returns a proxy prvalue. Every details-layer routine consequently receives the same object
// by reference. This proof is independent of whether a target ABI classifies a particular aggregate in registers;
// ABI-specific value transport can be selected at this boundary without introducing copies in the fallback graph.

template <typename instmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *read_some(instmtype &&instm, char_type *first, char_type *last)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::read_some_impl(insmref, first, last);
}

template <typename instmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void read_all(instmtype &&instm, char_type *first, char_type *last)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::read_all_impl(insmref, first, last);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte *read_some_bytes(instmtype &&instm, ::std::byte *first, ::std::byte *last)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::read_some_bytes_impl(insmref, first, last);
}
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void read_all_bytes(instmtype &&instm, ::std::byte *first, ::std::byte *last)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::read_all_bytes_impl(insmref, first, last);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_bytes(instmtype &&instm, io_scatter_t const *pscatter,
															 ::std::size_t len)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_read_some_bytes_impl(insmref, pscatter, len);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_read_all_bytes(instmtype &&instm, io_scatter_t const *pscatter, ::std::size_t len)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	::fast_io::details::scatter_read_all_bytes_impl(insmref, pscatter, len);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_read_some(
	instmtype &&instm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::input_stream_ref(instm))>::input_char_type> const
		*pscatter,
	::std::size_t len)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_read_some_impl(insmref, pscatter, len);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_read_all(
	instmtype &&instm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::input_stream_ref(instm))>::input_char_type> const
		*pscatter,
	::std::size_t len)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_read_all_impl(insmref, pscatter, len);
}

template <typename instmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *pread_some(instmtype &&instm, char_type *first, char_type *last, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::pread_some_impl(insmref, first, last, off);
}

template <typename instmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pread_all(instmtype &&instm, char_type *first, char_type *last, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::pread_all_impl(insmref, first, last, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte *pread_some_bytes(instmtype &&instm, ::std::byte *first, ::std::byte *last,
											   ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::pread_some_bytes_impl(insmref, first, last, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pread_all_bytes(instmtype &&instm, ::std::byte *first, ::std::byte *last,
									  ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::pread_all_bytes_impl(insmref, first, last, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some_bytes(instmtype &&instm, io_scatter_t const *pscatter,
															  ::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_pread_some_bytes_impl(insmref, pscatter, len, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pread_all_bytes(instmtype &&instm, io_scatter_t const *pscatter, ::std::size_t len,
											  ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	::fast_io::details::scatter_pread_all_bytes_impl(insmref, pscatter, len, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some(
	instmtype &&instm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::input_stream_ref(instm))>::input_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_pread_some_impl(insmref, pscatter, len, off);
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pread_all(
	instmtype &&instm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::input_stream_ref(instm))>::input_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(instm);
	return ::fast_io::details::scatter_pread_all_impl(insmref, pscatter, len, off);
}

} // namespace operations

} // namespace fast_io

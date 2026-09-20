#pragma once

/*
 * Public span-based input operations (`operations` namespace).
 *
 * These overloads preserve span shape and return the filled/remaining span
 * contract for `some` operations while sharing the same single
 * stream-normalization and primitive synthesis rules as pointer ranges. They
 * are transfer conveniences only and do not add scanner semantics.
 */


namespace fast_io::operations
{

// Span overloads are independent public boundaries. They normalize once and call the details layer directly instead
// of delegating to another public overload, which would perform a second stream-ref customization. `decltype(auto)`
// simultaneously owns a proxy prvalue and preserves a stable lvalue customization result.

/** @brief Partially fills a typed span and returns its filled prefix. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<char_type> read_some_span(outstmtype &&outstm, ::fast_io::containers::span<char_type> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::containers::span<char_type>(first, ::fast_io::details::read_some_impl(insmref, first, last));
}

/** @brief Fills an entire typed span or propagates input failure. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void read_all_span(outstmtype &&outstm, ::fast_io::containers::span<char_type> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::read_all_impl(insmref, first, last);
}

/** @brief Partially fills a mutable byte span and returns its filled prefix. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<::std::byte> read_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte> sp)
{
	// Input byte ranges are writable destinations. A const-qualified span would
	// make this overload both semantically invalid and impossible to dispatch to
	// the byte-read primitives.
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::containers::span<::std::byte>(first, ::fast_io::details::read_some_bytes_impl(insmref, first, last));
}

/** @brief Fills an entire mutable byte span or propagates input failure. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void read_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::read_all_bytes_impl(insmref, first, last);
}

/** @brief Partially fills byte scatters and returns scatter progress. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_read_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t const *;
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::details::scatter_read_some_bytes_impl(
		insmref, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
}

/** @brief Fills every byte-scatter destination or propagates failure. */
template <typename outstmtype>
inline constexpr void scatter_read_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t const *;
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::scatter_read_all_bytes_impl(
		insmref, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
}

/** @brief Partially fills typed scatters and returns scatter progress. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_read_some_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const *;
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::details::scatter_read_some_impl(
		insmref, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
}

/** @brief Fills every typed-scatter destination or propagates failure. */
template <typename outstmtype>
inline constexpr void scatter_read_all_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const *;
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::scatter_read_all_impl(
		insmref, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
}

/** @brief Partially fills a typed span from an explicit input offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<char_type> pread_some_span(outstmtype &&outstm, ::fast_io::containers::span<char_type> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::containers::span<char_type>(first, ::fast_io::details::pread_some_impl(insmref, first, last, off));
}

/** @brief Fills a typed span completely from an explicit input offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pread_all_span(outstmtype &&outstm, ::fast_io::containers::span<char_type> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::pread_all_impl(insmref, first, last, off);
}

/** @brief Partially fills a byte span from an explicit input offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<::std::byte> pread_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::containers::span<::std::byte>(first, ::fast_io::details::pread_some_bytes_impl(insmref, first, last, off));
}

/** @brief Fills a byte span completely from an explicit input offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pread_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::pread_all_bytes_impl(insmref, first, last, off);
}

/** @brief Partially fills byte scatters from an explicit input offset. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::details::scatter_pread_some_bytes_impl(insmref, sp.data(), sp.size(), off);
}

/** @brief Fills all byte scatters from an explicit input offset. */
template <typename outstmtype>
inline constexpr void scatter_pread_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::scatter_pread_all_bytes_impl(insmref, sp.data(), sp.size(), off);
}

/** @brief Partially fills typed scatters from an explicit input offset. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pread_some_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const> sp, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	return ::fast_io::details::scatter_pread_some_impl(insmref, sp.data(), sp.size(), off);
}

/** @brief Fills all typed scatters from an explicit input offset. */
template <typename outstmtype>
inline constexpr void scatter_pread_all_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(outstm))>::input_char_type> const> sp, ::fast_io::intfpos_t off)
{
	decltype(auto) insmref = ::fast_io::operations::input_stream_ref(outstm);
	::fast_io::details::scatter_pread_all_impl(insmref, sp.data(), sp.size(), off);
}

} // namespace fast_io::operations

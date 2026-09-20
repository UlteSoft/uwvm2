#pragma once

/*
 * Public pointer-range output operations (`operations` namespace).
 *
 * `write_some`, `write_all`, and their byte/positioned variants accept a user
 * stream handle, invoke `output_stream_ref` exactly once, validate character
 * compatibility, and enter the normalized primitive synthesis layer. They are
 * direct transfer APIs for existing memory ranges, not aliases of
 * `print_freestanding` and not formatting operations.
 *
 * The one normalization is owned by basic_output_operation_guard. This also
 * gives every pointer/scatter/positioned entry the same success-only checked
 * finish semantics for explicitly opted-in temporary owners.
 */


namespace fast_io
{

namespace operations
{

/** @brief Writes a bounded typed prefix and returns its first unwritten unit. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *write_some(outstmtype &&outstm, char_type const *first, char_type const *last)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			return ::fast_io::details::write_some_impl(outsm, first, last);
		});
}

/** @brief Writes an entire typed pointer range or propagates failure. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all(outstmtype &&outstm, char_type const *first, char_type const *last)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			::fast_io::details::write_all_impl(outsm, first, last);
		});
}

/** @brief Writes a bounded byte prefix and returns its first unwritten byte. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte const *write_some_bytes(outstmtype &&outstm, ::std::byte const *first,
													 ::std::byte const *last)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			return ::fast_io::details::write_some_bytes_impl(outsm, first, last);
		});
}

/** @brief Writes an entire byte pointer range or propagates failure. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all_bytes(outstmtype &&outstm, ::std::byte const *first, ::std::byte const *last)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			::fast_io::details::write_all_bytes_impl(outsm, first, last);
		});
}

/** @brief Partially writes byte scatters and returns scatter progress. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes(outstmtype &&outstm, io_scatter_t const *pscatter,
															  ::std::size_t len)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			return ::fast_io::details::scatter_write_some_bytes_impl(
				outsm, pscatter, len);
		});
}

/** @brief Writes every byte scatter entry or propagates failure. */
template <typename outstmtype>
inline constexpr void scatter_write_all_bytes(outstmtype &&outstm, io_scatter_t const *pscatter, ::std::size_t len)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			::fast_io::details::scatter_write_all_bytes_impl(
				outsm, pscatter, len);
		});
}

/** @brief Partially writes typed scatters and returns scatter progress. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_write_some(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			return ::fast_io::details::scatter_write_some_impl(
				outsm, pscatter, len);
		});
}

/** @brief Writes every typed scatter entry or propagates failure. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_write_all(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			::fast_io::details::scatter_write_all_impl(outsm, pscatter, len);
		});
}

/** @brief Partially writes a typed range at an explicit output offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *pwrite_some(outstmtype &&outstm, char_type const *first, char_type const *last,
											  ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::pwrite_some_impl(outsm, first, last, off);
	});
}

/** @brief Writes a complete typed range at an explicit output offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all(outstmtype &&outstm, char_type const *first, char_type const *last,
								 ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::pwrite_all_impl(outsm, first, last, off);
	});
}

/** @brief Partially writes a byte range at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte const *pwrite_some_bytes(outstmtype &&outstm, ::std::byte const *first,
													  ::std::byte const *last, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::pwrite_some_bytes_impl(outsm, first, last, off);
	});
}

/** @brief Writes a complete byte range at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all_bytes(outstmtype &&outstm, ::std::byte const *first, ::std::byte const *last,
									   ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::pwrite_all_bytes_impl(outsm, first, last, off);
	});
}

/** @brief Partially writes byte scatters at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes(outstmtype &&outstm, io_scatter_t const *pscatter,
															   ::std::size_t len, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, pscatter, len, off);
	});
}

/** @brief Writes all byte scatters at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pwrite_all_bytes(outstmtype &&outstm, io_scatter_t const *pscatter, ::std::size_t len,
											   ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::scatter_pwrite_all_bytes_impl(outsm, pscatter, len, off);
	});
}

/** @brief Partially writes typed scatters at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_pwrite_some_impl(outsm, pscatter, len, off);
	});
}

/** @brief Writes all typed scatters at an explicit output offset. */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pwrite_all(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_pwrite_all_impl(outsm, pscatter, len, off);
	});
}

/**
 * @brief Writes a single character to the output stream.
 * @tparam outstmtype The type of the output stream to write to, which should satisfy the output_stream concept.
 * @param outstm The output stream to write to.
 * @param ch The character to write to the output stream.
 * @note This function is marked constexpr, allowing its invocation in constant expressions.
 */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void char_put(outstmtype &&outstm,
							   typename ::std::remove_cvref_t<
								   decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type ch)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(
		guard, [&](auto &outsm) {
			::fast_io::details::char_put_impl(outsm, ch);
		});
}

} // namespace operations

} // namespace fast_io

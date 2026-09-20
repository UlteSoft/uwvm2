#pragma once

/*
 * Public span-based output operations (`operations` namespace).
 *
 * These overloads preserve span shape and return the unconsumed suffix for
 * `some` operations while sharing the same single stream-normalization and
 * primitive synthesis rules as pointer ranges. They are convenience front
 * doors over transfer CPOs; they do not add print semantics or allocation.
 * Their public lifetime boundary uses basic_output_operation_guard so return
 * shapes stay unchanged while opted-in temporary owners finish after success.
 */


namespace fast_io::operations
{

/** @brief Partially writes a typed span and returns its written prefix. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<char_type const> write_some_span(outstmtype &&outstm, ::fast_io::containers::span<char_type const> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::containers::span<char_type const>(
			first, ::fast_io::details::write_some_impl(outsm, first, last));
	});
}

/** @brief Writes every element of a typed span. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all_span(outstmtype &&outstm, ::fast_io::containers::span<char_type const> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::write_all_impl(outsm, first, last);
	});
}

/**
 * @brief Partially writes a byte span and returns its written prefix.
 *
 * The legacy character template slot remains defaulted for source compatibility
 * with code that explicitly named both historical template arguments.
 */
template <typename outstmtype, ::std::integral legacy_char_type = char>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<::std::byte const> write_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte const> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::containers::span<::std::byte const>(
			first, ::fast_io::details::write_some_bytes_impl(outsm, first, last));
	});
}

/** @brief Writes every byte of a span while retaining legacy template syntax. */
template <typename outstmtype, ::std::integral legacy_char_type = char>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte const> sp)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::write_all_bytes_impl(outsm, first, last);
	});
}

/** @brief Partially writes a span of byte scatters and returns progress. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t const *;
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_write_some_bytes_impl(
			outsm, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
	});
}

/** @brief Writes every entry in a span of byte scatters. */
template <typename outstmtype>
inline constexpr void scatter_write_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t const *;
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::scatter_write_all_bytes_impl(
			outsm, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
	});
}

/** @brief Partially writes a span of typed scatters and returns progress. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_span(
	outstmtype &&outstm,
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const>
		sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<typename ::std::remove_cvref_t<
			decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const *;
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_write_some_impl(
			outsm, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
	});
}

/** @brief Writes every entry in a span of typed scatters. */
template <typename outstmtype>
inline constexpr void scatter_write_all_span(
	outstmtype &&outstm,
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const>
		sp)
{
	using io_scatter_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<typename ::std::remove_cvref_t<
			decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const *;
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::scatter_write_all_impl(
			outsm, reinterpret_cast<io_scatter_may_alias_const_ptr>(sp.data()), sp.size());
	});
}

/** @brief Partially writes a typed span at an explicit output offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<char_type const> pwrite_some_span(outstmtype &&outstm, ::fast_io::containers::span<char_type const> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::containers::span<char_type const>(
			first, ::fast_io::details::pwrite_some_impl(outsm, first, last, off));
	});
}

/** @brief Writes a complete typed span at an explicit output offset. */
template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all_span(outstmtype &&outstm, ::fast_io::containers::span<char_type const> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::pwrite_all_impl(outsm, first, last, off);
	});
}

/** @brief Partially writes a byte span at an explicit output offset. */
template <typename outstmtype, ::std::integral legacy_char_type = char>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::containers::span<::std::byte const> pwrite_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte const> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::containers::span<::std::byte const>(
			first, ::fast_io::details::pwrite_some_bytes_impl(outsm, first, last, off));
	});
}

/** @brief Writes a complete byte span at an explicit output offset. */
template <typename outstmtype, ::std::integral legacy_char_type = char>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::std::byte const> sp, ::fast_io::intfpos_t off)
{
	auto first{sp.data()};
	auto last{first + sp.size()};
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::pwrite_all_bytes_impl(outsm, first, last, off);
	});
}

/** @brief Partially writes byte-scatter spans at an explicit offset. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, sp.data(), sp.size(), off);
	});
}

/** @brief Writes all byte-scatter spans at an explicit output offset. */
template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_span(outstmtype &&outstm, ::fast_io::containers::span<::fast_io::io_scatter_t const> sp, ::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::scatter_pwrite_all_bytes_impl(outsm, sp.data(), sp.size(), off);
	});
}

/** @brief Partially writes typed-scatter spans at an explicit offset. */
template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_span(
	outstmtype &&outstm,
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const>
		sp,
	::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		return ::fast_io::details::scatter_pwrite_some_impl(outsm, sp.data(), sp.size(), off);
	});
}

/** @brief Writes all typed-scatter spans at an explicit output offset. */
template <typename outstmtype>
inline constexpr void scatter_pwrite_all_span(
	outstmtype &&outstm,
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const>
		sp,
	::fast_io::intfpos_t off)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::details::scatter_pwrite_all_impl(outsm, sp.data(), sp.size(), off);
	});
}

} // namespace fast_io::operations

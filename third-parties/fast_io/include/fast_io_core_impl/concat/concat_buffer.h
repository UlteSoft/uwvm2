#pragma once

/*
 * Generic growable staging destination for concat (IO level).
 *
 * This buffer adapts concat's chosen printable strategy to destinations that
 * cannot be materialized safely in one direct contiguous pass. It owns growth
 * and final transfer into the requested strlike type, while element
 * normalization and representation still use the ordinary print protocols.
 * It is an IO implementation device, not a public string or formatting syntax.
 */

namespace fast_io::details
{

template <::std::integral ch_type>
struct basic_concat_buffer
{
	using char_type = ch_type;
	static inline constexpr ::std::size_t buffer_size{2048u / sizeof(ch_type)};
	char_type *buffer_begin, *buffer_curr, *buffer_end;
	char_type stack_buffer[buffer_size];
	inline constexpr basic_concat_buffer() noexcept
		: buffer_begin{stack_buffer}, buffer_curr{stack_buffer}, buffer_end{stack_buffer + buffer_size}
	{
	}
	inline basic_concat_buffer(basic_concat_buffer const &) = delete;
	inline basic_concat_buffer &operator=(basic_concat_buffer const &) = delete;
	inline
#if __cpp_constexpr_dynamic_alloc >= 201907L
	constexpr
#endif
		~basic_concat_buffer()
	{
		if (buffer_begin != stack_buffer) [[unlikely]]
		{
			deallocate_iobuf_space<false, ch_type>(buffer_begin, static_cast<::std::size_t>(buffer_end - buffer_begin));
		}
	}
};

/// @brief Proves concat's private staging put area only when every possible backing domain is cacheable.
/// @details The initial array is embedded in the owner, but reserve growth uses `native_thread_local_allocator` through
///          `allocate_iobuf_space`. That alias can name a user-supplied allocator in a configured build, so treating
///          the inline case as a blanket proof would become false after the first growth. The allocator concept mirrors
///          the exact backend chosen by allocation/impl.h and keeps custom/freestanding fallbacks unmarked unless they
///          opt in explicitly. This marker is write-only because `basic_concat_buffer` is exposed as a string-like
///          output destination. Its later range construction reads an already bounded prefix directly; a future read
///          prefetch at that site should carry a dedicated source projection rather than widen the output adapter.
template <::std::integral char_type>
	requires(::fast_io::prfch_cacheable_allocator_provenance<
			 ::fast_io::native_thread_local_allocator>)
inline constexpr ::std::true_type prfch_cacheable_write_provenance_define(
	::fast_io::io_type_t<basic_concat_buffer<char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr char_type *
strlike_begin(::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>,
			  ::fast_io::details::basic_concat_buffer<char_type> &str) noexcept
{
	return str.buffer_begin;
}

template <::std::integral char_type>
inline constexpr char_type *
strlike_curr(::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>,
			 ::fast_io::details::basic_concat_buffer<char_type> &str) noexcept
{
	return str.buffer_curr;
}

template <::std::integral char_type>
inline constexpr char_type *
strlike_end(::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>,
			::fast_io::details::basic_concat_buffer<char_type> &str) noexcept
{
	return str.buffer_end;
}

template <::std::integral char_type>
inline constexpr void
strlike_set_curr(::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>,
				 ::fast_io::details::basic_concat_buffer<char_type> &str, char_type *p) noexcept
{
	str.buffer_curr = p;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
basic_concat_buffer_strlike_reserve_cold_impl(::fast_io::details::basic_concat_buffer<char_type> &str, ::std::size_t n,
											  ::std::size_t df) noexcept
{
	auto old_buffer_begin_ptr{str.buffer_begin};
	bool onstack{old_buffer_begin_ptr == str.stack_buffer};
	auto newptr{allocate_iobuf_space<char_type>(n)};
	// newptr != nullptr
	::std::size_t const elements{static_cast<::std::size_t>(str.buffer_curr - str.buffer_begin)};
	auto newcurr_ptr{non_overlapped_copy_n(old_buffer_begin_ptr, elements, newptr)};
	str.buffer_curr = newcurr_ptr;
	if (!onstack)
	{
		deallocate_iobuf_space<false, char_type>(old_buffer_begin_ptr, df);
	}
	str.buffer_begin = newptr;
	str.buffer_end = newptr + n;
}

template <::std::integral char_type>
inline constexpr void basic_concat_buffer_strlike_reserve_impl(::fast_io::details::basic_concat_buffer<char_type> &str,
															   ::std::size_t n) noexcept
{
	::std::size_t df{static_cast<::std::size_t>(str.buffer_end - str.buffer_begin)};
	if (df < n) [[unlikely]]
	{
		basic_concat_buffer_strlike_reserve_cold_impl(str, n, df);
	}
}

template <::std::integral char_type>
inline constexpr void
strlike_reserve(::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>,
				::fast_io::details::basic_concat_buffer<char_type> &str, ::std::size_t n) noexcept
{
	basic_concat_buffer_strlike_reserve_impl(str, n);
}

template <::std::integral char_type>
inline constexpr ::std::size_t strlike_sso_size(
	::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>) noexcept
{
	return ::fast_io::details::basic_concat_buffer<char_type>::buffer_size;
}

template <::std::integral char_type>
inline constexpr ::std::true_type strlike_buffered_print_preferred(
	::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>) noexcept
{
	// This internal staging type owns a fixed inline area and grows into one retained allocation only on exhaustion.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	::fast_io::io_strlike_type_t<char_type, ::fast_io::details::basic_concat_buffer<char_type>>) noexcept
{
	// Cursor access is a direct read/write of the three internal pointers. No output/status customization is associated
	// with this private staging type, and no allocation can move until the ordinary overflow path is entered.
	return {};
}

template <::std::integral char_type>
inline constexpr io_strlike_reference_wrapper<char_type, ::fast_io::details::basic_concat_buffer<char_type>>
io_strlike_ref(io_alias_t, ::fast_io::details::basic_concat_buffer<char_type> &str) noexcept
{
	return {__builtin_addressof(str)};
}

} // namespace fast_io::details

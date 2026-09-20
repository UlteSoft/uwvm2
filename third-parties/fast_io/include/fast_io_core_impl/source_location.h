#pragma once

namespace fast_io
{

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
inline constexpr reserve_scatters_size_result
	print_reserve_scatters_size(::fast_io::io_reserve_type_t<char_type, ::std::source_location>) noexcept
{
	constexpr ::std::size_t ulint32_rsv_size{
		print_reserve_size(::fast_io::io_reserve_type<char_type, ::std::uint_least32_t>)};
	constexpr ::std::size_t total_uint_least32_t_rsv_size{ulint32_rsv_size * 2 + 3};
	return {3, total_uint_least32_t_rsv_size};
}

namespace details
{

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
inline constexpr basic_reserve_scatters_define_result<char_type>
prrsv_reserve_scatters_source_location_define_impl(::fast_io::basic_io_scatter_t<char_type> *pscatters,
												   char_type *pbuffer, ::std::source_location const &location) noexcept
{
	using chartypeconstaliasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type const *;
	if constexpr (::std::same_as<char_type, char>)
	{
		char const *fnm{location.file_name()};
		*pscatters = ::fast_io::basic_io_scatter_t<char_type>{fnm, ::fast_io::cstr_len(fnm)};
	}
	else
	{
		char_type const *fnm{reinterpret_cast<chartypeconstaliasptr>(location.file_name())};
		*pscatters = ::fast_io::basic_io_scatter_t<char_type>{fnm, ::fast_io::cstr_len(fnm)};
	}
	++pscatters;
	char_type *iter{pbuffer};
	*iter = ::fast_io::char_literal_v<u8':', char_type>;
	++iter;
	iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::uint_least32_t>, iter, location.line());
	*iter = ::fast_io::char_literal_v<u8':', char_type>;
	++iter;
	iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::uint_least32_t>, iter, location.column());
	*iter = ::fast_io::char_literal_v<u8':', char_type>;
	++iter;
	*pscatters = ::fast_io::basic_io_scatter_t<char_type>{pbuffer, static_cast<::std::size_t>(iter - pbuffer)};
	++pscatters;
	if constexpr (::std::same_as<char_type, char>)
	{
		char const *fnm{location.function_name()};
		*pscatters = ::fast_io::basic_io_scatter_t<char_type>{fnm, ::fast_io::cstr_len(fnm)};
	}
	else
	{
		char_type const *fnm{reinterpret_cast<chartypeconstaliasptr>(location.function_name())};
		*pscatters = ::fast_io::basic_io_scatter_t<char_type>{fnm, ::fast_io::cstr_len(fnm)};
	}
	++pscatters;
	return {pscatters, iter};
}

} // namespace details

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr basic_reserve_scatters_define_result<char_type>
print_reserve_scatters_define(::fast_io::io_reserve_type_t<char_type, ::std::source_location>,
							  ::fast_io::basic_io_scatter_t<char_type> *pscatters, char_type *pbuffer,
							  ::std::source_location const &location) noexcept
{
	return ::fast_io::details::prrsv_reserve_scatters_source_location_define_impl(pscatters, pbuffer, location);
}

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char_type, ::std::source_location>) noexcept
{
	// File/function names have implementation-managed static lifetime, while punctuation and coordinates are written
	// into the caller's reserve buffer.  No descriptor depends on mutable formatter scratch, so adjacent producers may
	// be materialized before the final scatter write.
	return {};
}

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	::fast_io::io_reserve_type_t<char_type, ::std::source_location>) noexcept
{
	// Copying source_location preserves the implementation-managed file/function strings and the scalar coordinates.
	// The only generated characters are written into caller-owned reserve storage, so no returned descriptor can name
	// either the original source_location object or its normalized copy. This stronger proof keeps ABI-small locations
	// on the value-decay path without weakening the general self-borrowing rule.
	return {};
}

template <::std::integral char_type>
	requires(sizeof(char_type) == 1)
inline constexpr ::std::true_type
print_semantic_optional_scatter_status_transparent_leaf(
	::fast_io::io_reserve_type_t<char_type, ::std::source_location>) noexcept
{
	// source_location has no user-extensible associated namespace, and fast_io owns its complete reserve-scatters
	// protocol. That protocol is non-throwing, preserves the file/line/column/function order, retains only
	// implementation-lifetime strings, and writes every generated coordinate into caller-owned reserve storage.
	// Optional descriptor compaction therefore cannot introduce a whole-record status owner, alter an exception-visible
	// prefix, or invalidate an earlier retained component.
	return {};
}

namespace manipulators
{

/// @brief Captures the caller's current `std::source_location` as a printable value.
/// @details The default argument is evaluated at the call site; passing an explicit location returns that location
///          unchanged. Formatting later emits the library-defined file/line/column/function representation.
inline
#ifdef __cpp_consteval
	consteval
#else
	constexpr
#endif
	::std::source_location
	cur_src_loc(::std::source_location loc = ::std::source_location::current()) noexcept
{
	return loc;
}

} // namespace manipulators

} // namespace fast_io

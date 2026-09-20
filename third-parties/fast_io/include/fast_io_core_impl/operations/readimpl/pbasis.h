#pragma once

/*
 * Positioned scalar input synthesis (primitive operation sublayer).
 *
 * This file derives typed and byte `pread_some`/`pread_all` operations from
 * positioned provider CPOs or valid seek-plus-sequential fallbacks, advancing
 * offsets with checked progress semantics. The same normalized observer is
 * borrowed for all attempts. Scanner selection and public scan reporting are
 * above this layer.
 */

namespace fast_io
{
namespace details
{

// Positional fallbacks borrow the same normalized observer as scalar reads. Offset advancement changes only the
// request coordinate; it is not a reason to reconstruct the device proxy. Keeping `insm` by reference also proves
// that a multi-attempt `pread_all` loop observes one proxy state and one synchronization domain throughout.
//
// An equal endpoint pair is the formal proof of an empty destination and may be represented by two null pointers.
// Empty scalar calls still cross the mutex/dispatch/CPO boundary because a provider may observe that request and its
// explicit offset. Consequently this layer never short-circuits solely on equality: it uses the shared input pointer
// helpers to map cardinality and returned-prefix progress without evaluating null subtraction or `nullptr + 0`.

/**
 * @brief Recognizes a copy-substitutable observer with a prvalue-callable scalar byte-pread leaf.
 * @details The ABI refinement includes the explicit ADL proof that observer
 *          copies denote the same external stream state. The expression test
 *          independently excludes providers whose semantics require a mutable
 *          observer identity. Neither object layout nor size can admit this
 *          path on its own.
 */
template <typename instmtype>
concept abi_value_direct_pread_some_bytes =
	::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &> &&
	requires(instmtype &insm, ::std::byte *ptr, ::fast_io::intfpos_t off) {
		{
			pread_some_bytes_underflow_define(instmtype{insm}, ptr, ptr, off)
		} -> ::std::same_as<::std::byte *>;
	};

template <typename instmtype>
	requires ::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte *
pread_some_bytes_abi_value_direct_impl(
	instmtype insm, ::std::byte *first, ::std::byte *last,
	::fast_io::intfpos_t off)
{
	return pread_some_bytes_underflow_define(
		instmtype{insm}, first, last, off);
}

/**
 * @brief Repeats a direct positioned byte-read leaf to initialize the complete range.
 * @details For each iteration, the primitive progress `d = distance(first,next)`
 *          advances both the destination prefix and the independent positional
 *          coordinate by exactly `d`. A zero-length proper prefix is EOF. This
 *          is the cold synthesis recurrence with only the proven observer
 *          transport changed from borrow to value. Equality is tested before
 *          any pointer-domain-dependent operation, so a null empty call remains
 *          observable while contributing exactly zero positional progress.
 */
template <typename instmtype>
	requires ::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
pread_all_bytes_abi_value_direct_impl(
	instmtype insm, ::std::byte *first, ::std::byte *last,
	::fast_io::intfpos_t off)
{
	for (;;)
	{
		auto next{pread_some_bytes_underflow_define(
			instmtype{insm}, first, last, off)};
		if (next == last)
		{
			return;
		}
		if (next == first)
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
		}
		off = ::fast_io::fposoffadd_nonegative(
			off, ::fast_io::details::input_pointer_distance(first, next));
		first = next;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr typename instmtype::input_char_type *
pread_some_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
					 typename instmtype::input_char_type *last, ::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>)
	{
		return pread_some_underflow_define(insm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		auto status{scatter_pread_some_underflow_define(insm, __builtin_addressof(sc), 1, off)};
		if (!status.position)
		{
			return ::fast_io::details::input_pointer_advance(
				first, status.position_in_scatter);
		}
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype>)
	{
		pread_all_underflow_define(insm, first, last, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype>)
	{
		basic_io_scatter_t<char_type> sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_pread_all_underflow_define(insm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>)
	{
		if constexpr (sizeof(typename instmtype::input_char_type) == 1)
		{
			using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= char_type *;
			::std::byte *firstptr{reinterpret_cast<::std::byte *>(first)};
			::std::byte *ptr{pread_some_bytes_cold_impl(insm, firstptr, reinterpret_cast<::std::byte *>(last), off)};
			// A one-byte typed range and its byte projection have identical cursor addresses. Returning the projected
			// cursor directly removes a redundant subtract/add pair and preserves a null zero-progress result verbatim.
			return reinterpret_cast<char_type_ptr>(ptr);
		}
		else
		{
			// Typed positional offsets count characters; byte customizations require byte coordinates. Convert at the
			// protocol boundary and keep all subsequent progress in bytes.
			::fast_io::intfpos_t byte_off{::fast_io::details::scatter_fpos_mul<char_type>(off)};
			::std::byte *firstptr{reinterpret_cast<::std::byte *>(first)};
			::std::byte *ptr{
				pread_some_bytes_cold_impl(insm, firstptr, reinterpret_cast<::std::byte *>(last), byte_off)};
			::std::ptrdiff_t ptdf{
				::fast_io::details::input_pointer_distance(firstptr, ptr)};
			::std::size_t diff{static_cast<::std::size_t>(ptdf)};
			::std::size_t v{diff / sizeof(char_type)};
			::std::size_t const partial{diff % sizeof(char_type)};
			if (partial != 0)
			{
				byte_off = ::fast_io::fposoffadd_nonegative(byte_off, ptdf);
				pread_all_bytes_cold_impl(
					insm, ptr,
					::fast_io::details::input_pointer_advance(
						ptr, sizeof(char_type) - partial),
					byte_off);
				++v;
			}
			return ::fast_io::details::input_pointer_advance(first, v);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::read_some_impl(insm, first, last)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(off), ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::read_some_impl(insm, first, last)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::std::byte *pread_some_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
														 ::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pread_some_bytes_underflow_define<instmtype>)
	{
		return pread_some_bytes_underflow_define(insm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_bytes_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		io_scatter_t sc{first, len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_pread_some_bytes_underflow_define(
				insm, __builtin_addressof(sc), 1, off), len)};
		return ::fast_io::details::input_pointer_advance(first, progress);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>))
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		auto const result{::fast_io::details::pread_some_cold_impl(
			insm, reinterpret_cast<char_type_ptr>(first), reinterpret_cast<char_type_ptr>(last), off)};
		return reinterpret_cast<::std::byte *>(result);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype>)
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_ptr>(first), len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_pread_some_underflow_define(
				insm, __builtin_addressof(sc), 1, off), len)};
		return ::fast_io::details::input_pointer_advance(first, progress);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype>)
	{
		pread_all_bytes_underflow_define(insm, first, last, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_pread_all_bytes_underflow_define(insm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype>)
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		pread_all_underflow_define(insm, reinterpret_cast<char_type_ptr>(first), reinterpret_cast<char_type_ptr>(last),
								   off);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype>)
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_ptr>(first),
										 ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_pread_all_underflow_define(insm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::read_some_bytes_impl(insm, first, last)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::read_some_impl(insm, first, last)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void pread_all_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
										  typename instmtype::input_char_type *last, ::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype>)
	{
		pread_all_underflow_define(insm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		scatter_pread_all_underflow_define(insm, __builtin_addressof(sc), 1, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto nit{pread_some_underflow_define(insm, first, last, off)};
			off = ::fast_io::fposoffadd_nonegative(
				off, ::fast_io::details::input_pointer_distance(first, nit));
			if (nit == last)
			{
				return;
			}
			if (nit == first)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			first = nit;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype>)
	{
		for (;;)
		{
			::std::size_t const len{
				::fast_io::details::input_pointer_range_size(first, last)};
			basic_io_scatter_t<char_type> sc{first, len};
			auto ret{scatter_pread_some_underflow_define(insm, __builtin_addressof(sc), 1, off)};
			auto const progress{
				::fast_io::scatter_status_one_size(ret, len)};
			auto nit{
				::fast_io::details::input_pointer_advance(first, progress)};
			off = ::fast_io::fposoffadd_nonegative(off, progress);
			if (nit == last)
			{
				return;
			}
			if (nit == first)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			first = nit;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>)
	{
		pread_all_bytes_cold_impl(insm, reinterpret_cast<::std::byte *>(first), reinterpret_cast<::std::byte *>(last),
								  ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::read_all_impl(insm, first, last);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(off), ::fast_io::seekdir::beg);
		::fast_io::details::read_all_bytes_impl(insm, reinterpret_cast<::std::byte *>(first), reinterpret_cast<::std::byte *>(last));
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void pread_all_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
												::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype>)
	{
		pread_all_bytes_underflow_define(insm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_pread_all_bytes_underflow_define(insm, __builtin_addressof(sc), 1, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_bytes_underflow_define<instmtype>)
	{
		while (first != last)
		{
			auto nit{pread_some_bytes_underflow_define(insm, first, last, off)};
			if (nit == last)
			{
				return;
			}
			if (nit == first)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			off = ::fast_io::fposoffadd_nonegative(
				off, ::fast_io::details::input_pointer_distance(first, nit));
			first = nit;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_bytes_underflow_define<instmtype>)
	{
		for (;;)
		{
			::std::size_t const len{
				::fast_io::details::input_pointer_range_size(first, last)};
			io_scatter_t sc{first, len};
			::std::size_t sz{::fast_io::scatter_status_one_size(
				scatter_pread_some_bytes_underflow_define(insm, __builtin_addressof(sc), 1, off), len)};
			auto nit{
				::fast_io::details::input_pointer_advance(first, sz)};
			if (nit == last)
			{
				return;
			}
			if (nit == first)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			off = ::fast_io::fposoffadd_nonegative(off, sz);
			first = nit;
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>)
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		char_type_ptr firstcptr{reinterpret_cast<char_type_ptr>(first)};
		char_type_ptr lastcptr{reinterpret_cast<char_type_ptr>(last)};
		::fast_io::details::pread_all_cold_impl(insm, firstcptr, lastcptr, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::read_all_bytes_impl(insm, first, last);
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::read_all_bytes_impl(insm, first, last);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename instmtype>
inline constexpr typename instmtype::input_char_type *
pread_some_impl(instmtype &insm, typename instmtype::input_char_type *first, typename instmtype::input_char_type *last,
				::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			// Position is an argument, not mutable stream state, but the device operation still belongs to the same
			// synchronization domain. Unwrap only after the complete protocol proves type progress and character identity.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::pread_some_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>)
		{
			auto first_bytes{reinterpret_cast<::std::byte *>(first)};
			auto result{::fast_io::details::pread_some_bytes_abi_value_direct_impl(
				insm, first_bytes, reinterpret_cast<::std::byte *>(last), off)};
			return ::fast_io::details::input_pointer_advance(
				first, static_cast<::std::size_t>(
					::fast_io::details::input_pointer_distance(
						first_bytes, result)));
		}
		return ::fast_io::details::pread_some_cold_impl(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr void pread_all_impl(instmtype &insm, typename instmtype::input_char_type *first,
									 typename instmtype::input_char_type *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::pread_all_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>)
		{
			return ::fast_io::details::pread_all_bytes_abi_value_direct_impl(
				insm, reinterpret_cast<::std::byte *>(first),
				reinterpret_cast<::std::byte *>(last), off);
		}
		::fast_io::details::pread_all_cold_impl(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
													::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::pread_some_bytes_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>)
		{
			return ::fast_io::details::pread_some_bytes_abi_value_direct_impl(
				insm, first, last, off);
		}
		return ::fast_io::details::pread_some_bytes_cold_impl(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr void pread_all_bytes_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
										   ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::pread_all_bytes_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_pread_some_bytes<instmtype>)
		{
			return ::fast_io::details::pread_all_bytes_abi_value_direct_impl(
				insm, first, last, off);
		}
		::fast_io::details::pread_all_bytes_cold_impl(insm, first, last, off);
	}
}

} // namespace details

} // namespace fast_io

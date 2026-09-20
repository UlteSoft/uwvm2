#pragma once

/*
 * Positioned scalar output synthesis (primitive operation sublayer).
 *
 * This file derives typed and byte `pwrite_some`/`pwrite_all` operations from
 * positioned provider CPOs or valid seek-plus-sequential fallbacks, while
 * advancing offsets with checked progress semantics. One normalized observer
 * is borrowed for the full algorithm. Formatting and stream-scenario policy
 * are strictly above this layer.
 */

namespace fast_io
{
namespace details
{

// The public positional scalar contract admits equal null endpoints as a mathematical empty source. Such a request is
// nevertheless observable to locking, flushing, seek synthesis, and the selected scalar CPO, so equality cannot be an
// eager-return condition here. The shared output pointer helpers encode the proof split: equality has cardinality zero
// without arithmetic, whereas every nonzero prefix retains the ordinary same-array requirement and offset recurrence.

/**
 * @brief Recognizes a copy-substitutable observer with a prvalue-callable scalar byte-pwrite leaf.
 * @details The ADL semantic marker proves that copied observers share every
 *          externally visible stream transition; the ABI refinement proves
 *          inexpensive direct argument transport; and the expression check
 *          rejects mutable-reference-only leaves. These are independent
 *          obligations, so a small trivial cursor is never copied by inference.
 */
template <typename outstmtype>
concept abi_value_direct_pwrite_some_bytes =
	::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &> &&
	requires(outstmtype &outsm, ::std::byte const *ptr,
			 ::fast_io::intfpos_t off) {
		{
			pwrite_some_bytes_overflow_define(
				outstmtype{outsm}, ptr, ptr, off)
		} -> ::std::same_as<::std::byte const *>;
	};

template <typename outstmtype>
	requires ::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte const *
pwrite_some_bytes_abi_value_direct_impl(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last,
	::fast_io::intfpos_t off)
{
	return pwrite_some_bytes_overflow_define(
		outstmtype{outsm}, first, last, off);
}

/**
 * @brief Repeats the direct positioned byte-write leaf until the full source is consumed.
 * @details The source prefix and independent file offset advance by the same
 *          primitive progress on every iteration, exactly matching the
 *          established cold synthesis recurrence. Repeated observer copies are
 *          permitted only by `abi_value_direct_pwrite_some_bytes`'s explicit
 *          substitution proof. The leaf is invoked once for an empty range, as
 *          in the borrowed recurrence, while equality-safe distance mapping
 *          prevents null subtraction and leaves the supplied offset unchanged.
 */
template <typename outstmtype>
	requires ::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
pwrite_all_bytes_abi_value_direct_impl(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last,
	::fast_io::intfpos_t off)
{
	for (;;)
	{
		auto next{pwrite_some_bytes_overflow_define(
			outstmtype{outsm}, first, last, off)};
		auto const progress{
			::fast_io::details::output_pointer_distance(first, next)};
		if (next == last)
		{
			return;
		}
		off = ::fast_io::fposoffadd_nonegative(off, progress);
		first = next;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr typename outstmtype::output_char_type const *
pwrite_some_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
					  typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		return pwrite_some_overflow_define(outsm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_pwrite_some_overflow_define(
				outsm, __builtin_addressof(sc), 1, off), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		pwrite_all_overflow_define(outsm, first, last, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>)
	{
		basic_io_scatter_t<char_type> sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_pwrite_all_overflow_define(outsm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
						   outstmtype> ||
					   ::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		if constexpr (sizeof(typename outstmtype::output_char_type) == 1)
		{
			using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= char_type const *;
			::std::byte const *firstptr{reinterpret_cast<::std::byte const *>(first)};
			::std::byte const *ptr{
				pwrite_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last), off)};
			// One-byte character and byte cursors are address-identical. Reinterpreting the leaf result keeps the ABI
			// return value in its original register and avoids undefined arithmetic for a null empty-range result.
			return reinterpret_cast<char_type_const_ptr>(ptr);
		}
		else
		{
			// The typed positional protocol measures off in characters, while every *_bytes customization measures it
			// in bytes. Multiplying exactly once at this boundary prevents successive wide-character writes from
			// overlapping; the saturating helper preserves the library's positional overflow policy.
			::fast_io::intfpos_t byte_off{::fast_io::details::scatter_fpos_mul<char_type>(off)};
			::std::byte const *firstptr{reinterpret_cast<::std::byte const *>(first)};
			::std::byte const *ptr{
				pwrite_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last), byte_off)};
			::std::ptrdiff_t ptdf{
				::fast_io::details::output_pointer_distance(firstptr, ptr)};
			::std::size_t diff{static_cast<::std::size_t>(ptdf)};
			::std::size_t v{diff / sizeof(char_type)};
			::std::size_t const partial{diff % sizeof(char_type)};
			if (partial != 0)
			{
				byte_off = ::fast_io::fposoffadd_nonegative(byte_off, ptdf);
				pwrite_all_bytes_cold_impl(
					outsm, ptr,
					::fast_io::details::output_pointer_advance(
						ptr, sizeof(char_type) - partial),
					byte_off);
				++v;
			}
			return ::fast_io::details::output_pointer_advance(first, v);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::write_some_impl(outsm, first, last)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::scatter_fpos_mul<char_type>(off), ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::write_some_impl(outsm, first, last)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::std::byte const *pwrite_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
																::std::byte const *last, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		return pwrite_some_bytes_overflow_define(outsm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		io_scatter_t sc{first, len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_pwrite_some_bytes_overflow_define(
				outsm, __builtin_addressof(sc), 1, off), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		auto const result{pwrite_some_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
											  reinterpret_cast<char_type_const_ptr>(last), off)};
		return reinterpret_cast<::std::byte const *>(result);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_const_ptr>(first), len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_pwrite_some_overflow_define(
				outsm, __builtin_addressof(sc), 1, off), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		pwrite_all_bytes_overflow_define(outsm, first, last, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_pwrite_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		pwrite_all_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
								   reinterpret_cast<char_type_const_ptr>(last), off);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_const_ptr>(first),
										 ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_pwrite_all_overflow_define(outsm, __builtin_addressof(sc), 1, off);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::write_some_bytes_impl(outsm, first, last)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::write_some_impl(outsm, first, last)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void pwrite_all_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
										   typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		pwrite_all_overflow_define(outsm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>)
	{
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		scatter_pwrite_all_overflow_define(outsm, __builtin_addressof(sc), 1, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		do
		{
			auto nit{pwrite_some_overflow_define(outsm, first, last, off)};
			off = ::fast_io::fposoffadd_nonegative(
				off, ::fast_io::details::output_pointer_distance(first, nit));
			first = nit;
		} while (first != last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		do
		{
			::std::size_t const len{
				::fast_io::details::output_pointer_range_size(first, last)};
			basic_io_scatter_t<char_type> sc{first, len};
			auto ret{scatter_pwrite_some_overflow_define(outsm, __builtin_addressof(sc), 1, off)};
			auto const progress{
				::fast_io::scatter_status_one_size(ret, len)};
			auto nit{
				::fast_io::details::output_pointer_advance(first, progress)};
			off = ::fast_io::fposoffadd_nonegative(off, progress);
			first = nit;
		} while (first != last);
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		pwrite_all_bytes_cold_impl(outsm, reinterpret_cast<::std::byte const *>(first),
								   reinterpret_cast<::std::byte const *>(last),
								   ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		::fast_io::details::write_all_impl(outsm, first, last);
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::scatter_fpos_mul<char_type>(off), ::fast_io::seekdir::beg);
		::fast_io::details::write_all_bytes_impl(outsm, reinterpret_cast<::std::byte const *>(first), reinterpret_cast<::std::byte const *>(last));
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void pwrite_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last,
												 ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		pwrite_all_bytes_overflow_define(outsm, first, last, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_pwrite_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		do
		{
			auto nit{pwrite_some_bytes_overflow_define(outsm, first, last, off)};
			off = ::fast_io::fposoffadd_nonegative(
				off, ::fast_io::details::output_pointer_distance(first, nit));
			first = nit;
		} while (first != last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		do
		{
			::std::size_t const len{
				::fast_io::details::output_pointer_range_size(first, last)};
			io_scatter_t sc{first, len};
			auto const progress{::fast_io::scatter_status_one_size(
				scatter_pwrite_some_bytes_overflow_define(
					outsm, __builtin_addressof(sc), 1, off), len)};
			auto nit{
				::fast_io::details::output_pointer_advance(first, progress)};
			off = ::fast_io::fposoffadd_nonegative(off, progress);
			first = nit;
		} while (first != last);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>))
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		char_type_const_ptr firstcptr{reinterpret_cast<char_type_const_ptr>(first)};
		char_type_const_ptr lastcptr{reinterpret_cast<char_type_const_ptr>(last)};
		::fast_io::details::pwrite_all_cold_impl(outsm, firstcptr, lastcptr, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		::fast_io::details::write_all_bytes_impl(outsm, first, last);
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>))
	{
		auto oldoff{::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, off, ::fast_io::seekdir::beg);
		::fast_io::details::write_all_bytes_impl(outsm, first, last);
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename outstmtype>
inline constexpr typename outstmtype::output_char_type const *
pwrite_some_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
				 typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			// Positional output bypasses stream-position mutation, not the object's synchronization contract. The lock
			// must therefore cover the recursive primitive exactly as it covers ordinary write.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::pwrite_some_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(outsm);
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_pwrite_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>)
		{
			auto first_bytes{reinterpret_cast<::std::byte const *>(first)};
			auto result{::fast_io::details::pwrite_some_bytes_abi_value_direct_impl(
				outsm, first_bytes, reinterpret_cast<::std::byte const *>(last), off)};
			return ::fast_io::details::output_pointer_advance(
				first, static_cast<::std::size_t>(
					::fast_io::details::output_pointer_distance(
						first_bytes, result)));
		}
		return ::fast_io::details::pwrite_some_cold_impl(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr void pwrite_all_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
									  typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::pwrite_all_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(outsm);
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_pwrite_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>)
		{
			return ::fast_io::details::pwrite_all_bytes_abi_value_direct_impl(
				outsm, reinterpret_cast<::std::byte const *>(first),
				reinterpret_cast<::std::byte const *>(last), off);
		}
		::fast_io::details::pwrite_all_cold_impl(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_impl(outstmtype &outsm, ::std::byte const *first,
														   ::std::byte const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::pwrite_some_bytes_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(outsm);
		}
		if constexpr (::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>)
		{
			return ::fast_io::details::pwrite_some_bytes_abi_value_direct_impl(
				outsm, first, last, off);
		}
		return ::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last,
											::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::pwrite_all_bytes_impl(unlocked, first, last, off);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(outsm);
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_pwrite_some_bytes<outstmtype>)
		{
			return ::fast_io::details::pwrite_all_bytes_abi_value_direct_impl(
				outsm, first, last, off);
		}
		::fast_io::details::pwrite_all_bytes_cold_impl(outsm, first, last, off);
	}
}

} // namespace details

} // namespace fast_io

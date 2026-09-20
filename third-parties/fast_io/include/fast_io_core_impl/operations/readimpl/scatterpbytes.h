/*
 * Positioned byte-scatter input synthesis (primitive operation sublayer).
 *
 * This is the native-byte counterpart of `scatterp.h`. It executes some/all
 * positioned scatter reads, advances both byte offsets and descriptor cursors,
 * and falls back only through capability-equivalent primitive paths. It
 * provides bytes to callers without interpreting them as scan syntax.
 */

namespace fast_io
{

namespace details
{

// Byte-position and descriptor progress are local algorithm state. They must not trigger additional observer value
// transports; every fallback in this file consequently accepts the boundary-owned observer by reference.
// A scalar fallback must still visit every descriptor. The shared mutable byte-range normalizer maps `{nullptr,0}` to
// equal pointers at a stable anchor, making its zero cardinality and unchanged offset well-defined without suppressing
// the provider call; positive extents continue to require a writable array exactly as the native scatter protocol does.

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_pread_all_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n,
														::fast_io::intfpos_t off);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters,
																		::std::size_t n, intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_bytes_underflow_define<instmtype>)
	{
		// The limit counts descriptors rather than bytes. Keep off unchanged for the one admitted native prefix; the
		// returned status remains sufficient for a positional all-loop to calculate the exact continuation offset.
		::std::size_t const count{
			::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
		return scatter_pread_some_bytes_underflow_define(insm, pscatters, count, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_bytes_underflow_define<instmtype>)
	{
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [baseb, len] = pscatters[i];
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(baseb))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			auto written{::fast_io::details::pread_some_bytes_impl(
				insm, range.first, range.last, off)};
			::std::ptrdiff_t const dfsz{
				::fast_io::details::input_pointer_distance(
					range.first, written)};
			::std::size_t const sz{static_cast<::std::size_t>(dfsz)};
			if (sz != len)
			{
				return {i, sz};
			}
			off = ::fast_io::fposoffadd_nonegative(off, dfsz);
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype> ||
					   ::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype>)
	{
		::fast_io::details::scatter_pread_all_bytes_cold_impl(insm, pscatters, n, off);
		return {n, 0};
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype> ||
						::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype> ||
						::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>))
	{
		// Some semantics admit this bounded prefix. One-byte width preserves status units, and the outlined adapter
		// supplies real typed descriptors without propagating its fixed workspace into dispatch callers.
		return ::fast_io::details::scatter_pread_some_bytes_via_typed_cold_impl(insm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::scatter_read_some_bytes_cold_impl(insm, pscatters, n)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto ret{::fast_io::details::scatter_read_some_bytes_cold_impl(insm, pscatters, n)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_impl(instmtype &insm, io_scatter_t const *pscatters,
																   ::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::scatter_pread_some_bytes_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pread_some_bytes_cold_impl(insm, pscatters, n, off);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_pread_all_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n,
														::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<instmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, instmtype>()};
		// The full-status {maximum,0} describes a completed byte batch. fposoffadd_scatters sums exactly that batch,
		// including zero for empty descriptors, before both the descriptor span and positional origin are advanced.
		while (maximum < n)
		{
			scatter_pread_all_bytes_underflow_define(insm, pscatters, maximum, off);
			off = ::fast_io::fposoffadd_scatters(off, pscatters, {maximum, 0u});
			pscatters += maximum;
			n -= maximum;
		}
		scatter_pread_all_bytes_underflow_define(insm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basep))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::pread_all_bytes_impl(
				insm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_bytes_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_pread_some_bytes_impl(insm, pscatters, n, off)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			::std::size_t pisc{ret.position_in_scatter};
			if (retpos == 0u && pisc == 0u)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			off = ::fast_io::fposoffadd_scatters(off, pscatters, ret);
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::std::byte *pibase{reinterpret_cast<::std::byte *>(const_cast<void *>(pi.base))};
				auto const range{
					::fast_io::details::scatter_to_input_scalar_range(
						pibase, pi.len)};
				::fast_io::details::pread_all_bytes_impl(
					insm,
					::fast_io::details::input_pointer_advance(
						range.first, pisc),
					range.last, off);
				off = ::fast_io::fposoffadd_nonegative(off, pi.len - pisc);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_bytes_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basep))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::pread_all_bytes_impl(
				insm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		// Consecutive materialized typed chunks initialize the byte ranges in order; checked advancement preserves the
		// one-byte positional coordinate without an unrelated-descriptor effective-type assumption.
		::fast_io::details::scatter_pread_all_bytes_via_typed_cold_impl(insm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::scatter_read_all_bytes_impl(insm, pscatters, n);
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::scatter_read_all_bytes_impl(insm, pscatters, n);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename instmtype>
inline constexpr void scatter_pread_all_bytes_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n,
												   ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::scatter_pread_all_bytes_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pread_all_bytes_cold_impl(insm, pscatters, n, off);
	}
}

} // namespace details

} // namespace fast_io

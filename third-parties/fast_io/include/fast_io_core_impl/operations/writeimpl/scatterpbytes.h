/*
 * Positioned byte-scatter output synthesis (primitive operation sublayer).
 *
 * This is the native-byte counterpart of `scatterp.h`. It executes some/all
 * positioned scatter requests, advances both byte offsets and descriptor
 * cursors, and falls back only through capability-equivalent primitive paths.
 * It consumes already-materialized bytes and is independent of print/concat.
 */

namespace fast_io
{

namespace details
{

// Byte-scatter scalarization is a semantic decomposition, not permission to discard empty descriptors. The shared
// output-range normalizer supplies a stable anchor for `{nullptr,0}`, after which equality-safe progress mapping proves
// that the explicit byte offset is unchanged. Every nonempty descriptor retains its original address and array bounds.

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_pwrite_all_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
														 ::std::size_t n, ::fast_io::intfpos_t off);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_pwrite_some_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		// A positional some-operation remains a single bounded backend attempt. The status is intentionally relative to
		// the admitted prefix, while off continues to denote the first byte of that prefix.
		::std::size_t const count{
			::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
		return scatter_pwrite_some_bytes_overflow_define(outsm, pscatters, count, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		// Advance the positional offset only after a descriptor has completed. If descriptor i is short, {i, sz}
		// represents all progress and the all-operation will compute the retry offset from that status.
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [baseb, len] = pscatters[i];
			::std::byte const *base{reinterpret_cast<::std::byte const *>(baseb)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			auto written{::fast_io::details::pwrite_some_bytes_impl(outsm, range.first, range.last, off)};
			::std::ptrdiff_t const dfsz{
				::fast_io::details::output_pointer_distance(
					range.first, written)};
			::std::size_t sz{static_cast<::std::size_t>(dfsz)};
			if (sz != len)
			{
				return {i, sz};
			}
			off = ::fast_io::fposoffadd_nonegative(off, dfsz);
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
						   outstmtype> ||
					   ::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		::fast_io::details::scatter_pwrite_all_bytes_cold_impl(outsm, pscatters, n, off);
		return {n, 0};
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>))
	{
		// A bounded materialized prefix preserves native typed positional scatter and exact one-byte status units. The
		// outlined adapter also keeps its descriptor workspace out of callers of this dispatch function.
		return ::fast_io::details::scatter_pwrite_some_bytes_via_typed_cold_impl(outsm, pscatters, n, off);
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
		auto ret{::fast_io::details::scatter_write_some_bytes_cold_impl(outsm, pscatters, n)};
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
		auto ret{::fast_io::details::scatter_write_some_bytes_cold_impl(outsm, pscatters, n)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
		return ret;
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_impl(outstmtype &outsm, io_scatter_t const *pscatters,
																	::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// The empty byte prefix is complete; do not flush or forward a zero-iovec positional request.
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_some_bytes_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pwrite_some_bytes_cold_impl(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_pwrite_all_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
														 ::std::size_t n, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, outstmtype>()};
		// Split only between descriptors, after a batch has been completed in full. Advancing off by the sum of that
		// batch proves positional equivalence to one unbounded pwritev. SIZE_MAX makes this loop a no-op; every finite
		// policy value is nonzero, so subtracting maximum guarantees forward progress.
		while (maximum < n)
		{
			scatter_pwrite_all_bytes_overflow_define(outsm, pscatters, maximum, off);
			off = ::fast_io::fposoffadd_scatters(off, pscatters, {maximum, 0u});
			pscatters += maximum;
			n -= maximum;
		}
		scatter_pwrite_all_bytes_overflow_define(outsm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte const *base{reinterpret_cast<::std::byte const *>(basep)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_bytes_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, pscatters, n, off)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			::std::size_t pisc{ret.position_in_scatter};
			off = ::fast_io::fposoffadd_scatters(off, pscatters, ret);
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::std::byte const *base{reinterpret_cast<::std::byte const *>(pi.base)};
				auto const range{
					::fast_io::details::scatter_to_scalar_range(base, pi.len)};
				::fast_io::details::pwrite_all_bytes_impl(
					outsm,
					::fast_io::details::output_pointer_advance(
						range.first, pisc),
					range.last, off);
				off = ::fast_io::fposoffadd_nonegative(off, pi.len - pisc);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte const *base{reinterpret_cast<::std::byte const *>(basep)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_bytes_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>))
	{
		// One-byte width proves identical offset and extent units; the adapter completes consecutive materialized chunks
		// and advances their checked positional origin without descriptor type punning.
		::fast_io::details::scatter_pwrite_all_bytes_via_typed_cold_impl(outsm, pscatters, n, off);
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
		::fast_io::details::scatter_write_all_bytes_impl(outsm, pscatters, n);
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
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
		::fast_io::details::scatter_write_all_bytes_impl(outsm, pscatters, n);
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_impl(outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n,
													::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// Match the typed positional operation's nonempty-native-call invariant.
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_all_bytes_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pwrite_all_bytes_cold_impl(outsm, pscatters, n, off);
	}
}

} // namespace details

} // namespace fast_io

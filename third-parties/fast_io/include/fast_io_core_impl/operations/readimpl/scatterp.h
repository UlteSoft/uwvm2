#pragma once

/*
 * Positioned typed scatter-input synthesis (primitive operation sublayer).
 *
 * The algorithms combine an explicit byte position with typed destination
 * descriptor progress, using positioned scatter CPOs when available and
 * coherent byte/contiguous fallbacks otherwise. Offset units and partial state
 * remain explicit. The normalized observer is borrowed once and no target
 * parsing occurs here.
 */

namespace fast_io
{

namespace details
{

// A positional scatter loop advances an explicit offset while borrowing one observer. This separation is deliberate:
// offset arithmetic belongs to request state, whereas proxy construction belongs exclusively to the public boundary.
// Scalarization also preserves one call per descriptor, including `{nullptr,0}`. The shared input-range normalizer
// substitutes a stable writable anchor only for that null-empty representation; its equal endpoints prove zero
// progress, so the following descriptor receives the same positional origin while the scalar CPO remains observable.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters,
																		::std::size_t n, ::fast_io::intfpos_t off);

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
inline constexpr void
scatter_pread_all_cold_impl(instmtype &insm,
							basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters, ::std::size_t n,
							::fast_io::intfpos_t off);

/// @brief Reads typed scatter data through byte-positional primitives while preserving an exact byte origin.
/// @details The caller proves byte-pread capability and supplies a byte-valued origin. When that origin comes from byte
///          seek it need not be character-aligned. Each byte-pread result has the exact pointer type required by
///          subtraction, and its semantic progress contract keeps it within the submitted destination. A partial
///          character is completed before typed status escapes, proving that every reported element has a fully
///          initialized object representation and that retrying from the status cannot overlap or skip bytes. Noinline
///          confines the bounded descriptor workspace to this cold protocol bridge on aggressive inliners.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some_typed_at_byte_offset_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (sizeof(char_type) == 1u)
	{
		// Some semantics permit returning a bounded descriptor prefix. Materializing that prefix retains one native
		// byte-scatter request without violating effective type or requiring any status-unit translation.
		constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::io_scatter_t converted[capacity];
		::fast_io::details::scatter_read_materialize_byte_descriptors(converted, pscatters, count);
		return ::fast_io::details::scatter_pread_some_bytes_cold_impl(
			insm, converted, count, byte_offset);
	}
	for (::std::size_t i{}; i != n; ++i)
	{
		auto [base_typed_const, length] = pscatters[i];
		char_type *const base_typed{const_cast<char_type *>(base_typed_const)};
		auto const range{
			::fast_io::details::scatter_to_input_scalar_range(
				base_typed, length)};
		::std::byte *const base{reinterpret_cast<::std::byte *>(range.first)};
		::std::byte *const end{reinterpret_cast<::std::byte *>(range.last)};
		::std::byte *read{
			::fast_io::details::pread_some_bytes_impl(insm, base, end, byte_offset)};
		::std::ptrdiff_t const byte_difference{
			::fast_io::details::input_pointer_distance(base, read)};
		byte_offset = ::fast_io::fposoffadd_nonegative(byte_offset, byte_difference);
		::std::size_t const partial_bytes{
			static_cast<::std::size_t>(byte_difference) % sizeof(char_type)};
		::std::size_t typed_progress{
			static_cast<::std::size_t>(byte_difference) / sizeof(char_type)};
		if (partial_bytes != 0u)
		{
			::std::size_t const remaining_bytes{sizeof(char_type) - partial_bytes};
			::fast_io::details::pread_all_bytes_impl(
				insm, read,
				::fast_io::details::input_pointer_advance(
					read, remaining_bytes),
				byte_offset);
			byte_offset = ::fast_io::fposoffadd_nonegative(byte_offset, remaining_bytes);
			++typed_progress;
		}
		if (typed_progress != length)
		{
			return {i, typed_progress};
		}
	}
	return {n, 0u};
}

/// @brief Completes typed scatter input through byte-positional primitives at an exact byte offset.
/// @details Each all-operation initializes its entire byte range. Checked length multiplication and saturating offset
///          addition prove that consecutive descriptor ranges are adjacent in the source and remain within the
///          library's positional domain, without assuming that the first byte belongs to an aligned character index.
///          Noinline prevents the fixed conversion array from growing hot seek-synthesis frames.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_pread_all_typed_at_byte_offset_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset)
{
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::fast_io::io_scatter_t converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_read_materialize_byte_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_pread_all_bytes_cold_impl(
			insm, converted, count, byte_offset);
		byte_offset = ::fast_io::fposoffadd_scatters(byte_offset, converted, {count, 0u});
		pscatters += count;
		n -= count;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_pread_some_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
							 ::std::size_t n, ::fast_io::intfpos_t off)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype>)
	{
		// Clamping the descriptor count does not change the positional origin: this one some-call still starts at off and
		// reports progress only within its admitted prefix. A caller that continues derives the next origin from status.
		::std::size_t const count{
			::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
		return scatter_pread_some_underflow_define(insm, pscatters, count, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>)
	{
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [basec, len] = pscatters[i];
			char_type *base{const_cast<char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			char_type *it{::fast_io::details::pread_some_impl(
				insm, range.first, range.last, off)};
			if (it != range.last)
			{
				return {i, ::fast_io::details::input_pointer_range_size(
							   range.first, it)};
			}
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype> ||
					   ::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype>)
	{
		::fast_io::details::scatter_pread_all_cold_impl(insm, pscatters, n, off);
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>)
	{
		// The capability branch proves a byte-positional input family. Convert the typed origin once, then retain byte
		// coordinates through all descriptor and partial-character progress accounting.
		return ::fast_io::details::scatter_pread_some_typed_at_byte_offset_cold_impl(
			insm, pscatters, n, ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		auto status{::fast_io::details::scatter_read_some_cold_impl(insm, pscatters, n)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return status;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(off), ::fast_io::seekdir::beg);
		auto status{::fast_io::details::scatter_read_some_cold_impl(insm, pscatters, n)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
		return status;
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t
scatter_pread_some_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
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
			return ::fast_io::details::scatter_pread_some_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pread_some_cold_impl(insm, pscatters, n, off);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_pread_all_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
							::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, instmtype>()};
		// A completed positional batch advances off by exactly the sum of its descriptor lengths. Empty descriptors
		// contribute zero yet still move the descriptor pointer, proving that a zero-payload maximum-sized prefix cannot
		// skip data or shift the following nonempty batch. Finite maxima are nonzero, guaranteeing loop progress.
		while (maximum < n)
		{
			scatter_pread_all_underflow_define(insm, pscatters, maximum, off);
			off = ::fast_io::fposoffadd_scatters(off, pscatters, {maximum, 0u});
			pscatters += maximum;
			n -= maximum;
		}
		scatter_pread_all_underflow_define(insm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pread_all_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			auto *base{const_cast<typename instmtype::input_char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::pread_all_impl(
				insm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_pread_some_impl(insm, pscatters, n, off)};
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
				auto *base{const_cast<typename instmtype::input_char_type *>(pi.base)};
				auto const range{
					::fast_io::details::scatter_to_input_scalar_range(
						base, pi.len)};
				::fast_io::details::pread_all_impl(
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
	else if constexpr (::fast_io::operations::decay::defines::has_pread_some_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			auto *base{const_cast<typename instmtype::input_char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::pread_all_impl(
				insm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		// All-read completion proves each byte range initialized. The helper keeps the converted origin and every
		// following descriptor extent in bytes, so no unit-changing reinterpretation participates in offset arithmetic.
		::fast_io::details::scatter_pread_all_typed_at_byte_offset_cold_impl(
			insm, pscatters, n, ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, off, ::fast_io::seekdir::beg);
		::fast_io::details::scatter_read_all_cold_impl(insm, pscatters, n);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		auto oldoff{::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<typename instmtype::input_char_type>(off),
			::fast_io::seekdir::beg);
		::fast_io::details::scatter_read_all_cold_impl(insm, pscatters, n);
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, oldoff, ::fast_io::seekdir::beg);
	}
}

template <typename instmtype>
inline constexpr void scatter_pread_all_impl(instmtype &insm,
											 basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
											 ::std::size_t n, ::fast_io::intfpos_t off)
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
			return ::fast_io::details::scatter_pread_all_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pread_all_cold_impl(insm, pscatters, n, off);
	}
}

} // namespace details

} // namespace fast_io

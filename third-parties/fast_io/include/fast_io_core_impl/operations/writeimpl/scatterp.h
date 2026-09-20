#pragma once

/*
 * Positioned typed scatter-output synthesis (primitive operation sublayer).
 *
 * The algorithms here combine an explicit byte position with typed scatter
 * descriptor progress, using positioned scatter CPOs when available and
 * coherent byte/contiguous fallbacks otherwise. Offset units and partial
 * descriptor state remain explicit. The normalized observer is borrowed once;
 * no formatting or destination selection occurs here.
 */

namespace fast_io
{

namespace details
{

// Positioned scalarization preserves descriptor order and one CPO call per descriptor. In particular, a null-empty
// descriptor contributes zero to the explicit offset but is represented by the shared stable typed anchor before a
// scalar provider sees it. This is representation normalization only: nonempty ranges and named empty bases retain
// their original provenance, and the status/offset recurrence is unchanged.

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_cold_impl(outstmtype &outsm,
																		 io_scatter_t const *pscatters, ::std::size_t n,
																		 ::fast_io::intfpos_t off);

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
inline constexpr void
scatter_pwrite_all_cold_impl(outstmtype &outsm,
							 basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							 ::std::size_t n, ::fast_io::intfpos_t off);

/// @brief Emits typed scatter data through byte-positional primitives without changing the byte origin's unit.
/// @details The caller proves a byte-pwrite primitive and supplies an already-byte-valued origin. A byte-seek consumer
///          may provide an origin which is not divisible by the character width, so converting it to a character index
///          and back is not semantics-preserving. Every scalar
///          result is constrained by the pwrite concept to be a pointer and by the primitive semantic contract to lie
///          in the submitted range. Completing a partial character before returning keeps the public status in typed
///          elements and prevents a later retry from splitting one object representation. The noinline boundary keeps
///          the bounded one-byte descriptor workspace out of hot seek-synthesis callers on compilers that inline cold
///          functions aggressively.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_typed_at_byte_offset_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (sizeof(char_type) == 1u)
	{
		// A real byte-descriptor array preserves effective type while retaining the native scatter request. A some-call
		// may legally expose only this bounded prefix, so no retry or status translation is required here.
		constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::io_scatter_t converted[capacity];
		::fast_io::details::scatter_materialize_byte_descriptors(converted, pscatters, count);
		return ::fast_io::details::scatter_pwrite_some_bytes_cold_impl(
			outsm, converted, count, byte_offset);
	}
	for (::std::size_t i{}; i != n; ++i)
	{
		auto [base_typed, length] = pscatters[i];
		auto const range{::fast_io::details::scatter_to_scalar_range(base_typed, length)};
		::std::byte const *const base{reinterpret_cast<::std::byte const *>(range.first)};
		::std::byte const *const end{reinterpret_cast<::std::byte const *>(range.last)};
		::std::byte const *written{
			::fast_io::details::pwrite_some_bytes_impl(outsm, base, end, byte_offset)};
		::std::ptrdiff_t const byte_difference{
			::fast_io::details::output_pointer_distance(base, written)};
		byte_offset = ::fast_io::fposoffadd_nonegative(byte_offset, byte_difference);
		::std::size_t const partial_bytes{
			static_cast<::std::size_t>(byte_difference) % sizeof(char_type)};
		::std::size_t typed_progress{
			static_cast<::std::size_t>(byte_difference) / sizeof(char_type)};
		if (partial_bytes != 0u)
		{
			::std::size_t const remaining_bytes{sizeof(char_type) - partial_bytes};
			::fast_io::details::pwrite_all_bytes_impl(
				outsm, written,
				::fast_io::details::output_pointer_advance(
					written, remaining_bytes),
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

/// @brief Completes typed scatter output through byte-positional primitives at an exact byte offset.
/// @details Normal return from each byte all-operation proves full consumption. Checked multiplication establishes the
///          byte extent of every typed descriptor, and saturating addition preserves the positional arithmetic policy;
///          consequently consecutive calls cover exactly the original typed sequence even for an unaligned origin.
///          Noinline confines the fixed conversion array to this exceptional protocol bridge.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_pwrite_all_typed_at_byte_offset_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset)
{
	constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
	::fast_io::io_scatter_t converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_materialize_byte_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_pwrite_all_bytes_cold_impl(
			outsm, converted, count, byte_offset);
		byte_offset = ::fast_io::fposoffadd_scatters(byte_offset, converted, {count, 0u});
		pscatters += count;
		n -= count;
	}
}

// Keep the typed and byte positional helpers distinct. basic_io_scatter_t<char_type>::len is measured in char_type
// elements, whereas io_scatter_t::len is measured in bytes. Even when sizeof(char_type) == 1, layout equality is not
// an effective-type proof; the byte helper receives materialized io_scatter_t objects.

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_pwrite_some_cold_impl(outstmtype &outsm,
							  basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							  ::std::size_t n, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		// Like non-positional scatter "some", this is one native attempt over one legal prefix. Passing off here is
		// essential: positional output must neither depend on nor mutate the stream's current file position.
		::std::size_t const count{
			::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
		return scatter_pwrite_some_overflow_define(outsm, pscatters, count, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		// The offset for descriptor i equals the initial offset plus all preceding, fully written descriptor lengths.
		// On a partial write we return immediately without advancing off; the status carries the exact partial progress
		// and lets the enclosing all-operation derive the retry offset once, avoiding double advancement.
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [base, len] = pscatters[i];
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			auto written{::fast_io::details::pwrite_some_impl(outsm, range.first, range.last, off)};
			::std::size_t const sz{
				::fast_io::details::output_pointer_range_size(
					range.first, written)};
			if (sz != len)
			{
				return {i, sz};
			}
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		// Do not dispatch typed descriptors to scatter_pwrite_all_bytes_cold_impl: for wide character types that would
		// reinterpret character counts as byte counts. The typed helper preserves both descriptor and offset units.
		::fast_io::details::scatter_pwrite_all_cold_impl(outsm, pscatters, n, off);
		return {n, 0};
	}
	else
#if 0
		if constexpr ((::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
#endif
	/*
	 * The implementation of synthesizing pwrite through write+seek is missing
	 */
	{
		// The enclosing concept branch proves a byte-positional primitive. Convert its typed coordinate exactly once,
		// then keep the helper in byte units for every descriptor and partial-character completion.
		return ::fast_io::details::scatter_pwrite_some_typed_at_byte_offset_cold_impl(
			outsm, pscatters, n, ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t
scatter_pwrite_some_impl(outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
						 ::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// Empty positional output neither touches the stream nor interprets the supplied offset.
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// Locking changes only the observer used for dispatch; the caller's positional coordinate is forwarded unchanged.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_some_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pwrite_some_cold_impl(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_pwrite_all_cold_impl(outstmtype &outsm,
							 basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							 ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>)
	{
		using char_type = typename outstmtype::output_char_type;
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, outstmtype>()};
		// Every overflow all-call completes its consecutive batch. fposoffadd_scatters therefore advances off by
		// exactly that batch before the descriptor pointer moves, so each byte/character is written at the same logical
		// position as in one unbounded call. SIZE_MAX disables batching; a finite maximum is nonzero by concept, hence
		// both n and the unprocessed descriptor suffix decrease monotonically.
		while (maximum < n)
		{
			scatter_pwrite_all_overflow_define(outsm, pscatters, maximum, off);
			off = ::fast_io::fposoffadd_scatters(off, pscatters, {maximum, 0u});
			pscatters += maximum;
			n -= maximum;
		}
		scatter_pwrite_all_overflow_define(outsm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_pwrite_some_impl(outsm, pscatters, n, off)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			off = ::fast_io::fposoffadd_scatters(off, pscatters, ret);
			::std::size_t pisc{ret.position_in_scatter};
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				auto const range{
					::fast_io::details::scatter_to_scalar_range(
						pi.base, pi.len)};
				::fast_io::details::pwrite_all_impl(
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
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else
#if 0
if constexpr ((::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
#endif
	/*
	 * The implementation of synthesizing pwrite through write+seek is missing
	 */
	{
		using char_type = typename outstmtype::output_char_type;
		// Normal return from the byte all-primitive is the completion proof. The helper retains byte coordinates after
		// this single typed-to-byte origin conversion and advances each following descriptor by its checked byte extent.
		::fast_io::details::scatter_pwrite_all_typed_at_byte_offset_cold_impl(
			outsm, pscatters, n, ::fast_io::details::scatter_fpos_mul<char_type>(off));
	}
}

template <typename outstmtype>
inline constexpr void
scatter_pwrite_all_impl(outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
						::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// A zero-descriptor positional all-operation is complete without a lock, flush, or native syscall.
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// The unlocked observer is an implementation detail of synchronization, not a new positional origin.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_all_impl(unlocked, pscatters, n, off);
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
		return ::fast_io::details::scatter_pwrite_all_cold_impl(outsm, pscatters, n, off);
	}
}

} // namespace details

} // namespace fast_io

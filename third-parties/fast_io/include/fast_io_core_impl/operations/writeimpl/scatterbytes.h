/*
 * Sequential byte-scatter output synthesis (primitive operation sublayer).
 *
 * This file bridges native byte descriptors and one-byte typed scatter
 * protocols with bounded descriptor materialization, then implements exact
 * some/all progress. It is a representation adapter between device primitive
 * CPOs, not the print-level scatter formatter and not a source lifetime proof.
 */

namespace fast_io
{

namespace details
{

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_cold_impl(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t offset);

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_cold_impl(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t offset);

/// @brief Converts byte descriptors to one-byte typed descriptors by value.
/// @details Character width one proves equality of extent units, but not alias compatibility between descriptor
///          classes. Member-wise construction gives each destination object the exact effective type consumed by typed
///          scatter primitives while preserving every payload pointer and length.
template <::std::integral char_type>
inline constexpr void scatter_materialize_one_byte_typed_descriptors(
	::fast_io::basic_io_scatter_t<char_type> *destination, ::fast_io::io_scatter_t const *source,
	::std::size_t count) noexcept
{
	static_assert(sizeof(char_type) == 1u);
	for (::std::size_t i{}; i != count; ++i)
	{
		destination[i] = {static_cast<char_type const *>(source[i].base), source[i].len};
	}
}

/// @brief Executes one byte-scatter some request through a one-byte typed protocol with bounded stack storage.
/// @details The noinline cold boundary is a placement requirement: it keeps the one-KiB conversion array out of hot
///          scalar and buffered callers. Some semantics permit the converted prefix to be shorter than the request.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_write_some_bytes_via_typed_cold_impl(
	outstmtype &outsm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	::fast_io::details::scatter_materialize_one_byte_typed_descriptors(converted, pscatters, count);
	return ::fast_io::details::scatter_write_some_cold_impl(outsm, converted, count);
}

/// @brief Completes byte-scatter output through consecutive one-byte typed descriptor chunks.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_write_all_bytes_via_typed_cold_impl(
	outstmtype &outsm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_materialize_one_byte_typed_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_write_all_impl(outsm, converted, count);
		pscatters += count;
		n -= count;
	}
}

/// @brief Executes one byte-scatter some request through typed positional output at the supplied equal-unit offset.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_via_typed_cold_impl(
	outstmtype &outsm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n,
	::fast_io::intfpos_t offset)
{
	using char_type = typename outstmtype::output_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	::fast_io::details::scatter_materialize_one_byte_typed_descriptors(converted, pscatters, count);
	return ::fast_io::details::scatter_pwrite_some_cold_impl(outsm, converted, count, offset);
}

/// @brief Completes byte-scatter output through typed positional chunks while preserving the one-byte coordinate.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_pwrite_all_bytes_via_typed_cold_impl(
	outstmtype &outsm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n,
	::fast_io::intfpos_t offset)
{
	using char_type = typename outstmtype::output_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_byte_conversion_stack_capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_materialize_one_byte_typed_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_pwrite_all_cold_impl(outsm, converted, count, offset);
		offset = ::fast_io::fposoffadd_scatters(offset, converted, {count, 0u});
		pscatters += count;
		n -= count;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_write_some_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
																		::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		// Keep "some" as one bounded backend attempt. The returned status is relative to this legal prefix; an
		// all-operation, rather than the backend customization, owns any subsequent batching and retry policy.
		::std::size_t const count{
			::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
		return scatter_write_some_bytes_overflow_define(outsm, pscatters, count);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		// Descriptors before i were completed, while sz is the byte progress in descriptor i. This is precisely the
		// {complete-prefix, partial-next-descriptor} representation required by io_scatter_status_t.
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [baseb, len] = pscatters[i];
			::std::byte const *base{reinterpret_cast<::std::byte const *>(baseb)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			auto written{::fast_io::details::write_some_bytes_impl(outsm, range.first, range.last)};
			::std::size_t sz{static_cast<::std::size_t>(written - range.first)};
			if (sz != len)
			{
				return {i, sz};
			}
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		::fast_io::details::scatter_write_all_bytes_cold_impl(outsm, pscatters, n);
		return {n, 0};
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>))
	{
		// One-byte width proves equal payload and status units. The outlined adapter materializes a legal typed prefix
		// without allowing its bounded descriptor array to inflate this frequently instantiated dispatch function.
		return ::fast_io::details::scatter_write_some_bytes_via_typed_cold_impl(outsm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{scatter_pwrite_some_bytes_cold_impl(outsm, pscatters, n, current_position)};
		// The branch concepts prove byte seek plus byte-positional output, and scatter status semantically denotes a
		// prefix of this descriptor array. Using the queried origin for pwrite and then seeking by that prefix's checked
		// byte length is exactly the state transition of sequential scatter output.
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::fposoffadd_scatters(0, pscatters, ret), ::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::scatter_pwrite_some_bytes_via_typed_cold_impl(
			outsm, pscatters, n, current_position)};
		// `sizeof(char_type)==1` proves byte and character offsets are identical. Together with the exact typed pwrite
		// result/status protocol, member-wise descriptor conversion proves that the final prefix delta has the seek
		// operation's character unit without relying on layout aliasing.
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, ::fast_io::fposoffadd_scatters(0, pscatters, ret),
															   ::fast_io::seekdir::cur);
		return ret;
	}
	else
	{
		// With only a finite character put area and no overflow primitive, the cold byte suffix cannot advance.  A
		// zero status lets the hot some-operation report precisely the descriptors it already buffered.
		return {};
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes_impl(outstmtype &outsm, io_scatter_t const *pscatters,
																   ::std::size_t n)
{
	if (n == 0u)
	{
		// An empty byte-descriptor prefix succeeds without crossing a synchronization or device boundary.
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_write_some_bytes_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
	{
		using char_type = typename outstmtype::output_char_type;
		char_type *curr{obuffer_curr(outsm)};
		char_type *ed{obuffer_end(outsm)};

		// Byte descriptors share the typed put area when one output character is
		// one byte; its lazy all-null representation therefore denotes zero capacity.
		::std::size_t buffptrdiff{
			::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [base, len] = *i;
			// Exact fit is valid: copying `len == buffptrdiff` commits the cursor to the put-area end and completes
			// this descriptor without invoking an overflow primitive.
			if (len <= buffptrdiff) [[likely]]
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				curr =
					::fast_io::details::non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(base), len, curr);
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		obuffer_set_curr(outsm, curr);
		if (i != e) [[unlikely]]
		{
			auto ret{
				::fast_io::details::scatter_write_some_bytes_cold_impl(outsm, i, static_cast<::std::size_t>(e - i))};
			ret.position += static_cast<::std::size_t>(i - pscatters);
			return ret;
		}
		return {n, 0};
	}
	else
	{
		if constexpr (
			::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::scatter_write_some_bytes_abi_value_direct_impl(
				outsm, pscatters, n);
		}
		return ::fast_io::details::scatter_write_some_bytes_cold_impl(outsm, pscatters, n);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_write_all_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
														::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, outstmtype>()};
		// An all-operation may issue several native writes, but every batch is a consecutive, fully consumed prefix.
		// Removing that prefix therefore preserves the byte stream and eventually leaves one final legal batch.
		// SIZE_MAX denotes no declared limit and skips the loop; any real limit is nonzero by concept, proving progress.
		while (maximum < n)
		{
			scatter_write_all_bytes_overflow_define(outsm, pscatters, maximum);
			pscatters += maximum;
			n -= maximum;
		}
		scatter_write_all_bytes_overflow_define(outsm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte const *base{reinterpret_cast<::std::byte const *>(basep)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::write_all_bytes_impl(outsm, range.first, range.last);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_write_some_bytes_impl(outsm, pscatters, n)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			::std::size_t pisc{ret.position_in_scatter};
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::std::byte const *base{reinterpret_cast<::std::byte const *>(pi.base)};
				::fast_io::details::write_all_bytes_impl(outsm, base + pisc, base + pi.len);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte const *base{reinterpret_cast<::std::byte const *>(basep)};
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::write_all_bytes_impl(outsm, range.first, range.last);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>))
	{
		// Complete typed chunks preserve ordering and exact one-byte lengths. The outlined conversion boundary is part
		// of the performance contract: the fixed descriptor workspace must remain outside hot buffered callers.
		::fast_io::details::scatter_write_all_bytes_via_typed_cold_impl(outsm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		scatter_pwrite_all_bytes_cold_impl(outsm, pscatters, n, current_position);
		// Normal return proves complete positional byte output and positional I/O preserves current position. Advancing
		// from the queried origin by the checked full scatter extent therefore publishes exactly the sequential result.
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::scatter_pwrite_all_bytes_via_typed_cold_impl(
			outsm, pscatters, n, current_position);
		// One-byte character width proves descriptor lengths and offsets retain their values under the typed view. The
		// member-wise conversion gives that view its correct effective type, and typed positional all consumes the full
		// range without moving current position. The following complete extent is therefore the exact sequential delta.
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
inline constexpr void scatter_write_all_bytes_impl(outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	if (n == 0u)
	{
		// Preserve the same nonempty-native-call invariant as the typed scatter operation.
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_write_all_bytes_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
	{
		using char_type = typename outstmtype::output_char_type;
		char_type *curr{obuffer_curr(outsm)};
		char_type *ed{obuffer_end(outsm)};

		// Preserve all-operation ordering while mapping an unallocated put area to
		// zero capacity without undefined subtraction of its null cursor pair.
		::std::size_t buffptrdiff{
			::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [base, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				curr =
					::fast_io::details::non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(base), len, curr);
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		obuffer_set_curr(outsm, curr);
		if (i != e)
#if __has_cpp_attribute(unlikely)
			[[unlikely]]
#endif
		{
			return ::fast_io::details::scatter_write_all_bytes_cold_impl(outsm, i, static_cast<::std::size_t>(e - i));
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype> &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::scatter_write_all_bytes_abi_value_direct_impl(
				outsm, pscatters, n);
		}
		return ::fast_io::details::scatter_write_all_bytes_cold_impl(outsm, pscatters, n);
	}
}

} // namespace details

} // namespace fast_io

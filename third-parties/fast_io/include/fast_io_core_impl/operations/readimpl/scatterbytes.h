/*
 * Sequential byte-scatter input synthesis (primitive operation sublayer).
 *
 * This file bridges native byte destinations and one-byte typed scatter
 * protocols with bounded descriptor materialization, then implements exact
 * some/all progress. It adapts primitive device capability shapes; it is not a
 * scanner protocol and grants no extra destination lifetime or bounds.
 */

namespace fast_io
{

namespace details
{

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_cold_impl(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t offset);

template <typename instmtype>
inline constexpr void scatter_pread_all_bytes_cold_impl(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t offset);

/// @brief Converts byte input descriptors to one-byte typed descriptors by value.
/// @details Equal element width proves equal length units only. Constructing each typed descriptor member-wise gives
///          the callee objects of its declared type and retains the writable payload address later recovered by the
///          typed read protocol; reinterpreting the descriptor array would not establish that effective type.
template <::std::integral char_type>
inline constexpr void scatter_read_materialize_one_byte_typed_descriptors(
	::fast_io::basic_io_scatter_t<char_type> *destination, ::fast_io::io_scatter_t const *source,
	::std::size_t count) noexcept
{
	static_assert(sizeof(char_type) == 1u);
	for (::std::size_t i{}; i != count; ++i)
	{
		destination[i] = {static_cast<char_type const *>(source[i].base), source[i].len};
	}
}

/// @brief Executes one byte-scatter some request through a one-byte typed input protocol.
/// @details Some semantics allow a bounded prefix. The noinline cold boundary keeps the fixed conversion workspace
///          outside hot scalar and buffered callers while member-wise construction establishes descriptor type safety.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_bytes_via_typed_cold_impl(
	instmtype &insm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	::fast_io::details::scatter_read_materialize_one_byte_typed_descriptors(converted, pscatters, count);
	return ::fast_io::details::scatter_read_some_cold_impl(insm, converted, count);
}

/// @brief Completes byte-scatter input through consecutive one-byte typed descriptor chunks.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_read_all_bytes_via_typed_cold_impl(
	instmtype &insm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_read_materialize_one_byte_typed_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_read_all_impl(insm, converted, count);
		pscatters += count;
		n -= count;
	}
}

/// @brief Executes one byte-scatter some request through typed positional input at an equal-unit offset.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_pread_some_bytes_via_typed_cold_impl(
	instmtype &insm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n,
	::fast_io::intfpos_t offset)
{
	using char_type = typename instmtype::input_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	::fast_io::details::scatter_read_materialize_one_byte_typed_descriptors(converted, pscatters, count);
	return ::fast_io::details::scatter_pread_some_cold_impl(insm, converted, count, offset);
}

/// @brief Completes byte-scatter input through typed positional chunks while preserving one-byte coordinates.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_pread_all_bytes_via_typed_cold_impl(
	instmtype &insm, ::fast_io::io_scatter_t const *pscatters, ::std::size_t n,
	::fast_io::intfpos_t offset)
{
	using char_type = typename instmtype::input_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::fast_io::basic_io_scatter_t<char_type> converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_read_materialize_one_byte_typed_descriptors(converted, pscatters, count);
		::fast_io::details::scatter_pread_all_cold_impl(insm, converted, count, offset);
		offset = ::fast_io::fposoffadd_scatters(offset, converted, {count, 0u});
		pscatters += count;
		n -= count;
	}
}

// Byte scatter adaptation preserves the owner established by the public entry point. Reinterpreting descriptors may
// change the payload unit, but never the lifetime or transport policy of the normalized input observer.

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters,
																	   ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_bytes_underflow_define<instmtype>)
	{
		// Byte and typed descriptors share one native count limit: element width changes payload units, not the number
		// of iovec-like entries. A some-operation exposes only this admitted prefix to its backend.
		::std::size_t const count{
			::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
		return scatter_read_some_bytes_underflow_define(insm, pscatters, count);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_bytes_underflow_define<instmtype>)
	{
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [baseb, len] = pscatters[i];
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(baseb))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			auto written{::fast_io::details::read_some_bytes_impl(
				insm, range.first, range.last)};
			::std::size_t const sz{
				::fast_io::details::input_pointer_range_size(range.first, written)};
			if (sz != len)
			{
				return {i, sz};
			}
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype> ||
					   ::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype>)
	{
		::fast_io::details::scatter_read_all_bytes_cold_impl(insm, pscatters, n);
		return {n, 0};
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		// One-byte width proves equal payload and status units. The outlined adapter materializes a legal typed prefix
		// without allowing its bounded descriptor array to inflate this frequently instantiated dispatch function.
		return ::fast_io::details::scatter_read_some_bytes_via_typed_cold_impl(insm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{scatter_pread_some_bytes_cold_impl(insm, pscatters, n, current_position)};
		// Byte seek and byte-pread concepts share one coordinate, while status semantically proves a prefix of the
		// submitted writable ranges. Reading at the queried origin and advancing by that checked prefix exactly simulates
		// sequential scatter input without exposing positional I/O's unchanged cursor.
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::fposoffadd_scatters(0, pscatters, ret), ::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::scatter_pread_some_bytes_via_typed_cold_impl(
			insm, pscatters, n, current_position)};
		// One-byte character width proves equality of byte and typed coordinates and descriptor extents. The typed pread
		// status over member-wise converted descriptors is therefore also a valid byte-prefix proof, so the final seek
		// publishes exactly initialized input progress without a layout-aliasing assumption.
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, ::fast_io::fposoffadd_scatters(0, pscatters, ret),
															  ::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_bytes_impl(instmtype &insm, io_scatter_t const *pscatters,
																  ::std::size_t n)
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
			return ::fast_io::details::scatter_read_some_bytes_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		auto curr{ibuffer_curr(insm)};
		auto ed{ibuffer_end(insm)};

		::std::size_t buffptrdiff{
			::fast_io::details::ibuffer_remaining_size(curr, ed)};
		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [base, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type *;
				::fast_io::details::non_overlapped_copy_n(
					curr, len, reinterpret_cast<char_type_ptr>(const_cast<void *>(base)));
				curr = ::fast_io::details::input_pointer_advance(curr, len);
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		ibuffer_set_curr(insm, curr);
		if (i != e)
#if __has_cpp_attribute(unlikely)
			[[unlikely]]
#endif
		{
			auto ret{::fast_io::details::scatter_read_some_bytes_cold_impl(insm, i, static_cast<::std::size_t>(e - i))};
			ret.position += static_cast<::std::size_t>(i - pscatters);
			return ret;
		}
		return {n, 0};
	}
	else
	{
		if constexpr (
			::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::scatter_read_some_bytes_abi_value_direct_impl(
				insm, pscatters, n);
		}
		return ::fast_io::details::scatter_read_some_bytes_cold_impl(insm, pscatters, n);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_read_all_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, instmtype>()};
		// A completed batch establishes an exact descriptor boundary at which the byte request may be partitioned. The
		// concatenation of these consecutive batches is the original scatter list, including zero-length descriptors.
		while (maximum < n)
		{
			scatter_read_all_bytes_underflow_define(insm, pscatters, maximum);
			pscatters += maximum;
			n -= maximum;
		}
		scatter_read_all_bytes_underflow_define(insm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basep))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::read_all_bytes_impl(
				insm, range.first, range.last);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_bytes_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_read_some_bytes_impl(insm, pscatters, n)};
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
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(pi.base))};
				::fast_io::details::read_all_bytes_impl(insm, base + pisc, base + pi.len);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_bytes_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basep, len] = *i;
			::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basep))};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::read_all_bytes_impl(
				insm, range.first, range.last);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		// Complete typed chunks initialize exactly the original byte ranges. The outlined conversion boundary is part of
		// the performance contract: the fixed descriptor workspace must remain outside hot buffered callers.
		::fast_io::details::scatter_read_all_bytes_via_typed_cold_impl(insm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		scatter_pread_all_bytes_cold_impl(insm, pscatters, n, current_position);
		// Byte all-pread completion proves every destination byte initialized from the queried origin while leaving the
		// stream cursor unchanged. The complete checked scatter extent is consequently the exact sequential advance.
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::scatter_pread_all_bytes_via_typed_cold_impl(
			insm, pscatters, n, current_position);
		// With one-byte characters, the typed positional all-contract initializes precisely the byte descriptor ranges at
		// the queried character origin. Member-wise conversion proves descriptor typing; positional input does not move
		// current position, so advancing by the complete scatter extent reconstructs the sequential operation.
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
}

template <typename instmtype>
inline constexpr void scatter_read_all_bytes_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
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
			return ::fast_io::details::scatter_read_all_bytes_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		auto curr{ibuffer_curr(insm)};
		auto ed{ibuffer_end(insm)};

		::std::size_t buffptrdiff{
			::fast_io::details::ibuffer_remaining_size(curr, ed)};

		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [base, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type *;
				::fast_io::details::non_overlapped_copy_n(
					curr, len, reinterpret_cast<char_type_ptr>(const_cast<void *>(base)));
				curr = ::fast_io::details::input_pointer_advance(curr, len);
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		ibuffer_set_curr(insm, curr);
		if (i != e)
#if __has_cpp_attribute(unlikely)
			[[unlikely]]
#endif
		{
			return ::fast_io::details::scatter_read_all_bytes_cold_impl(insm, i, static_cast<::std::size_t>(e - i));
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype> &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::scatter_read_all_bytes_abi_value_direct_impl(
				insm, pscatters, n);
		}
		return ::fast_io::details::scatter_read_all_bytes_cold_impl(insm, pscatters, n);
	}
}

} // namespace details

} // namespace fast_io

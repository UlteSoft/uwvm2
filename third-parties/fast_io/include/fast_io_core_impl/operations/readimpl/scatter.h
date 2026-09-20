#pragma once

/*
 * Sequential typed scatter-input synthesis (primitive operation sublayer).
 *
 * Character scatter destinations are filled through the strongest coherent
 * scatter/contiguous, typed/byte CPO set exposed by the normalized input
 * observer. The algorithms maintain descriptor and intra-descriptor progress
 * for some/all completion. They supply characters to higher layers but never
 * interpret a scannable target.
 */

namespace fast_io
{


namespace operations::decay
{
// These declarations name the explicit owner ABI. Internal decomposition uses
// the borrowed/dispatch entries defined with the complete decay matrix.
template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_decay(instmtype insm, ::std::byte *first, ::std::byte *last);

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_decay(instmtype insm, ::std::byte *first, ::std::byte *last,
													 ::fast_io::intfpos_t);

} // namespace operations::decay

namespace details
{

// Scatter decomposition is an internal control-flow choice. All descriptor segments must therefore use one borrowed
// observer: N descriptors may require N scalar calls, but they must not create N proxy owners or change observer
// identity between calls. Native scatter CPOs still receive the ordinary named lvalue required by their protocol.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters,
																	   ::std::size_t n);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_read_all_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_read_all_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
						   ::std::size_t n);

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t offset);

template <typename instmtype>
inline constexpr void scatter_pread_all_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t offset);

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_typed_at_byte_offset_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset);

template <typename instmtype>
inline constexpr void scatter_pread_all_typed_at_byte_offset_cold_impl(
	instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset);

/**
 * @brief Recognizes a value-safe observer with a prvalue-callable native byte-scatter read leaf.
 * @details The stream-ref ABI concept carries the author-provided copy
 *          substitution proof; the expression requirement separately proves
 *          that this exact scatter provider accepts value transport. Thus an
 *          inline cursor cannot enter merely because it shares the same layout
 *          as a descriptor or file-handle proxy.
 */
template <typename instmtype>
concept abi_value_direct_scatter_read_some_bytes_leaf =
	::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &> &&
	requires(instmtype &insm, ::fast_io::io_scatter_t const *scatters,
			 ::std::size_t count) {
		{
			scatter_read_some_bytes_underflow_define(
				instmtype{insm}, scatters, count)
		} -> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename instmtype>
concept abi_value_direct_scatter_read_some_bytes =
	::fast_io::details::abi_value_direct_scatter_read_some_bytes_leaf<instmtype>;

/**
 * @brief Executes a native byte-scatter leaf with proven value semantics.
 * @details The provider's descriptor-count limit and one-attempt `some`
 *          contract are unchanged. Restricting this path to a native scatter
 *          CPO avoids duplicating the much larger scalar synthesis graph.
 */
template <typename instmtype>
	requires ::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::io_scatter_status_t
scatter_read_some_bytes_abi_value_direct_impl(
	instmtype insm, ::fast_io::io_scatter_t const *pscatters,
	::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	::std::size_t const count{
		::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
	return scatter_read_some_bytes_underflow_define(
		instmtype{insm}, pscatters, count);
}

/**
 * @brief Completes byte scatters through the value-safe `some` recurrence.
 * @details A native short scatter result advances by its proven prefix; a
 *          partial descriptor is completed through the scalar value leaf,
 *          after which the next iteration starts at an exact descriptor
 *          boundary. Zero progress before completion is the same EOF condition
 *          as the borrowed cold graph.
 */
template <typename instmtype>
	requires (::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype> &&
			  ::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
scatter_read_all_bytes_abi_value_direct_impl(
	instmtype insm, ::fast_io::io_scatter_t const *pscatters,
	::std::size_t n)
{
	for (;;)
	{
		auto const result{
			::fast_io::details::scatter_read_some_bytes_abi_value_direct_impl(
				insm, pscatters, n)};
		::std::size_t completed{result.position};
		if (completed == n)
		{
			return;
		}
		::std::size_t const partial{result.position_in_scatter};
		if (completed == 0u && partial == 0u)
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
		}
		if (partial != 0u)
		{
			auto const descriptor{pscatters[completed]};
			auto *base{reinterpret_cast<::std::byte *>(
				const_cast<void *>(descriptor.base))};
			::fast_io::details::read_all_bytes_abi_value_direct_impl(
				insm, base + partial, base + descriptor.len);
			++completed;
		}
		pscatters += completed;
		n -= completed;
	}
}

inline constexpr ::std::size_t scatter_read_byte_conversion_stack_capacity{
	1024u / sizeof(::fast_io::io_scatter_t) == 0u ? 1u : 1024u / sizeof(::fast_io::io_scatter_t)};

template <typename instmtype>
using scatter_read_some_byte_adapter_stream_t = ::std::conditional_t<
	::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype>, instmtype, instmtype &>;

template <typename instmtype>
using scatter_read_all_byte_adapter_stream_t = ::std::conditional_t<
	::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype> &&
		::fast_io::details::abi_value_direct_read_some_bytes<instmtype>,
	instmtype, instmtype &>;

/// @brief Materializes byte descriptors for input without violating descriptor effective type.
/// @details The source pointer is preserved, while checked multiplication converts each element count to bytes. The
///          destination array consists of actual `io_scatter_t` objects; equal layout never serves as an aliasing proof.
template <::std::integral char_type>
inline constexpr void scatter_read_materialize_byte_descriptors(
	::fast_io::io_scatter_t *destination, ::fast_io::basic_io_scatter_t<char_type> const *source,
	::std::size_t count) noexcept
{
	for (::std::size_t i{}; i != count; ++i)
	{
		destination[i] = {
			source[i].base,
			::fast_io::details::intrinsics::mul_or_overflow_die(source[i].len, sizeof(char_type))};
	}
}

/// @brief Performs one-byte typed-to-byte scatter input adaptation outside the caller's hot frame.
/// @details Some semantics allow the bounded converted prefix to be returned directly. A semantically marked native
///          observer is transported by value; every other observer remains borrowed. This single conditional signature
///          keeps the one-KiB workspace out of the caller without duplicating the adapter template.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_via_byte_descriptors_cold_impl(
	::fast_io::details::scatter_read_some_byte_adapter_stream_t<instmtype> insm,
	::fast_io::basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::io_scatter_t converted[capacity];
	::fast_io::details::scatter_read_materialize_byte_descriptors(
		converted, pscatters, count);
	if constexpr (::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype>)
	{
		return ::fast_io::details::scatter_read_some_bytes_abi_value_direct_impl(insm, converted, count);
	}
	else
	{
		return ::fast_io::details::scatter_read_some_bytes_cold_impl(insm, converted, count);
	}
}

/// @brief Performs one-byte typed-to-byte scatter all adaptation outside the caller's hot frame.
/// @details Each byte operation completes one consecutive materialized chunk. The conditional stream parameter uses
///          value transport only when both native scatter and scalar completion leaves carry explicit semantic proofs.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_read_all_via_byte_descriptors_cold_impl(
	::fast_io::details::scatter_read_all_byte_adapter_stream_t<instmtype> insm,
	::fast_io::basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
	::std::size_t n)
{
	constexpr ::std::size_t capacity{::fast_io::details::scatter_read_byte_conversion_stack_capacity};
	::fast_io::io_scatter_t converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_read_materialize_byte_descriptors(
			converted, pscatters, count);
		if constexpr (
			::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype> &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			::fast_io::details::scatter_read_all_bytes_abi_value_direct_impl(insm, converted, count);
		}
		else
		{
			::fast_io::details::scatter_read_all_bytes_cold_impl(insm, converted, count);
		}
		pscatters += count;
		n -= count;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_read_some_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
							::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		// "Some" performs one native attempt. Admit only the stream's legal descriptor prefix and leave the returned
		// status in the caller's descriptor coordinate system; a completed bounded prefix is therefore continuation,
		// not an implicit request to consume the suffix in this operation.
		::std::size_t const count{
			::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
		return scatter_read_some_underflow_define(insm, pscatters, count);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [basec, len] = pscatters[i];
			char_type *base{const_cast<char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			auto written{::fast_io::details::read_some_impl(
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
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype> ||
					   ::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		scatter_read_all_cold_impl(insm, pscatters, n);
		return {n, 0};
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		if constexpr (sizeof(char_type) == 1)
		{
			// One-byte width preserves status units; the adapter establishes descriptor effective type and bounds stack use.
			return ::fast_io::details::scatter_read_some_via_byte_descriptors_cold_impl<instmtype>(insm, pscatters, n);
		}
		else
		{
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basefd, len] = pscatters[i];
				char_type *basef{const_cast<char_type *>(basefd)};
				auto const range{
					::fast_io::details::scatter_to_input_scalar_range(basef, len)};
				::std::byte *base{reinterpret_cast<::std::byte *>(range.first)};
				::std::byte *ed{reinterpret_cast<::std::byte *>(range.last)};
				auto readed{::fast_io::details::read_some_bytes_impl(insm, base, ed)};
				::std::size_t const diff{
					::fast_io::details::input_pointer_range_size(base, readed)};
				::std::size_t md{diff % sizeof(char_type)};
				::std::size_t sz{diff / sizeof(char_type)};
				if (md)
				{
					::std::size_t dfd{sizeof(char_type) - md};
					::fast_io::details::read_all_bytes_impl(insm, readed, readed + dfd);
					++sz;
				}
				if (sz != len)
				{
					return {i, sz};
				}
			}
			return {n, 0};
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{scatter_pread_some_cold_impl(insm, pscatters, n, current_position)};
		// Typed seek and pread concepts establish one character coordinate. The returned status is a prefix proof for the
		// submitted descriptor range, so advancing current by its checked extent makes the synthesized operation
		// observationally identical to a sequential scatter read at the queried origin.
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, fposoffadd_scatters(0, pscatters, ret),
															  ::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::scatter_pread_some_typed_at_byte_offset_cold_impl(
			insm, pscatters, n, current_position)};
		// Byte seek gives no character-alignment proof. The byte-offset helper therefore preserves the exact origin and
		// completes any partially initialized character before reporting typed scatter progress; converting that proven
		// prefix once supplies the precise sequential byte delta.
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, ret)),
			::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t
scatter_read_some_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
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
			// Character preservation proves that descriptors formed for the locked stream retain their element type
			// after unwrapping. The guard surrounds recursive dispatch, so descriptor fallback cannot relock per segment.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::scatter_read_some_impl(unlocked, pscatters, n);
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
			auto [basec, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				char_type *base{const_cast<char_type *>(basec)};
				::fast_io::details::non_overlapped_copy_n(curr, len, base);
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
			auto ret{::fast_io::details::scatter_read_some_cold_impl(insm, i, static_cast<::std::size_t>(e - i))};
			ret.position += static_cast<::std::size_t>(i - pscatters);
			return ret;
		}
		return {n, 0};
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::scatter_read_some_via_byte_descriptors_cold_impl<instmtype>(
				insm, pscatters, n);
		}
		return scatter_read_some_cold_impl(insm, pscatters, n);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_read_all_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
						   ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, instmtype>()};
		// Every all-CPO either completes its admitted prefix or reports EOF by its normal failure mechanism. Splitting
		// only after a completed descriptor batch preserves order and observable destination contents. SIZE_MAX makes
		// the loop inactive for an unbounded stream; a finite policy is nonzero, so n decreases monotonically.
		while (maximum < n)
		{
			scatter_read_all_underflow_define(insm, pscatters, maximum);
			pscatters += maximum;
			n -= maximum;
		}
		scatter_read_all_underflow_define(insm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			char_type *base{const_cast<char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::read_all_impl(
				insm, range.first, range.last);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_read_some_impl(insm, pscatters, n)};
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
				char_type *base{const_cast<char_type *>(pi.base)};
				::fast_io::details::read_all_impl(insm, base + pisc, base + pi.len);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			char_type *base{const_cast<char_type *>(basec)};
			auto const range{
				::fast_io::details::scatter_to_input_scalar_range(base, len)};
			::fast_io::details::read_all_impl(
				insm, range.first, range.last);
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		if constexpr (sizeof(char_type) == 1)
		{
			// Complete materialized chunks preserve descriptor order without an unrelated effective-type view.
			::fast_io::details::scatter_read_all_via_byte_descriptors_cold_impl<instmtype>(insm, pscatters, n);
		}
		else
		{
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basef, len] = pscatters[i];
				auto *mutable_base{const_cast<char_type *>(basef)};
				auto const range{
					::fast_io::details::scatter_to_input_scalar_range(mutable_base, len)};
				::std::byte *base{reinterpret_cast<::std::byte *>(range.first)};
				::std::byte *ed{reinterpret_cast<::std::byte *>(range.last)};
				::fast_io::details::read_all_bytes_impl(insm, base, ed);
			}
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		scatter_pread_all_cold_impl(insm, pscatters, n, current_position);
		// Positional all-read completion proves that every descriptor has been initialized at the queried character
		// origin while current position remained unchanged. Advancing by the complete checked extent publishes exactly
		// the sequential state transition.
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::scatter_pread_all_typed_at_byte_offset_cold_impl(
			insm, pscatters, n, current_position);
		// The helper initializes the whole typed range from the exact byte origin, including unaligned origins. Positional
		// reads do not mutate current position, and the complete status is implicit in normal return; one checked
		// character-to-byte conversion is therefore the exact final seek delta.
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, {n, 0})),
			::fast_io::seekdir::cur);
	}
}

template <typename instmtype>
inline constexpr void scatter_read_all_impl(instmtype &insm,
											basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
											::std::size_t n)
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
			return ::fast_io::details::scatter_read_all_impl(unlocked, pscatters, n);
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
			auto [basec, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				char_type *base{const_cast<char_type *>(basec)};
				::fast_io::details::non_overlapped_copy_n(curr, len, base);
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
			return ::fast_io::details::scatter_read_all_cold_impl(insm, i, static_cast<::std::size_t>(e - i));
		}
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_scatter_read_some_bytes<instmtype> &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::scatter_read_all_via_byte_descriptors_cold_impl<instmtype>(
				insm, pscatters, n);
		}
		return ::fast_io::details::scatter_read_all_cold_impl(insm, pscatters, n);
	}
}

} // namespace details

} // namespace fast_io

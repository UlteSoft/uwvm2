#pragma once

/*
 * Sequential typed scatter-output synthesis (primitive operation sublayer).
 *
 * Character scatter requests are executed through the strongest coherent
 * scatter/contiguous, typed/byte CPO set exposed by the normalized output
 * observer. The algorithms maintain descriptor and intra-descriptor progress
 * for some/all completion. Scatter lifetime and element extents are caller
 * contracts; no printable-object representation is selected here.
 */

namespace fast_io
{

namespace details
{
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_write_some_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
																		::std::size_t n);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_write_all_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
														::std::size_t n);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_write_all_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters, ::std::size_t n);

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t offset);

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t offset);

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_typed_at_byte_offset_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset);

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_typed_at_byte_offset_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t byte_offset);

/**
 * @brief Recognizes a value-safe observer with a prvalue-callable native byte-scatter write leaf.
 * @details Admission requires both the stream author's copy-substitution
 *          marker and the target ABI's direct-argument policy. The expression
 *          requirement then proves that this exact provider accepts a prvalue;
 *          object size or triviality alone never authorizes cursor copying.
 */
template <typename outstmtype>
concept abi_value_direct_scatter_write_some_bytes_leaf =
	::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &> &&
	requires(outstmtype &outsm, ::fast_io::io_scatter_t const *scatters,
			 ::std::size_t count) {
		{
			scatter_write_some_bytes_overflow_define(
				outstmtype{outsm}, scatters, count)
		} -> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename outstmtype>
concept abi_value_direct_scatter_write_some_bytes =
	::fast_io::details::abi_value_direct_scatter_write_some_bytes_leaf<outstmtype>;

/**
 * @brief Executes a native byte-scatter output leaf with proven value semantics.
 * @details The provider's descriptor-count limit and single-call `some`
 *          contract are unchanged. Restricting this path to native scatter
 *          avoids cloning the substantially larger scalar synthesis graph.
 */
template <typename outstmtype>
	requires ::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::io_scatter_status_t
scatter_write_some_bytes_abi_value_direct_impl(
	outstmtype outsm, ::fast_io::io_scatter_t const *pscatters,
	::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	::std::size_t const count{
		::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
	return scatter_write_some_bytes_overflow_define(
		outstmtype{outsm}, pscatters, count);
}

/**
 * @brief Completes byte scatters through the proven value-safe `some` recurrence.
 * @details Each status removes an exact byte prefix. A partial descriptor is
 *          completed with the scalar value leaf before advancing to the next
 *          descriptor, preserving the borrowed graph's ordering and retry
 *          semantics while keeping the observer in its register ABI class.
 */
template <typename outstmtype>
	requires (::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype> &&
			  ::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
scatter_write_all_bytes_abi_value_direct_impl(
	outstmtype outsm, ::fast_io::io_scatter_t const *pscatters,
	::std::size_t n)
{
	for (;;)
	{
		auto const result{
			::fast_io::details::scatter_write_some_bytes_abi_value_direct_impl(
				outsm, pscatters, n)};
		::std::size_t completed{result.position};
		if (completed == n)
		{
			return;
		}
		::std::size_t const partial{result.position_in_scatter};
		if (partial != 0u)
		{
			auto const descriptor{pscatters[completed]};
			auto const *base{reinterpret_cast<::std::byte const *>(descriptor.base)};
			::fast_io::details::write_all_bytes_abi_value_direct_impl(
				outsm, base + partial, base + descriptor.len);
			++completed;
		}
		pscatters += completed;
		n -= completed;
	}
}

/// @brief Maximum temporary descriptor storage used to cross from typed to byte scatter protocols.
/// @details One KiB keeps the cold adapter bounded independently of the caller's descriptor count. An all-operation
///          drains consecutive chunks; a some-operation may legally report the first completed chunk as partial
///          progress. Consequently neither operation needs a dynamic allocation merely to establish the destination
///          descriptor's effective type.
inline constexpr ::std::size_t scatter_byte_conversion_stack_bytes{1024u};
inline constexpr ::std::size_t scatter_byte_conversion_stack_capacity{
	scatter_byte_conversion_stack_bytes / sizeof(::fast_io::io_scatter_t) == 0u
		? 1u
		: scatter_byte_conversion_stack_bytes / sizeof(::fast_io::io_scatter_t)};

template <typename outstmtype>
using scatter_write_some_byte_adapter_stream_t = ::std::conditional_t<
	::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype>, outstmtype, outstmtype &>;

template <typename outstmtype>
using scatter_write_all_byte_adapter_stream_t = ::std::conditional_t<
	::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype> &&
		::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>,
	outstmtype, outstmtype &>;

/// @brief Converts typed scatter values into genuine byte scatter objects.
/// @details The source and destination descriptor classes can have identical layouts without being alias-compatible.
///          Member-wise construction gives every destination element the correct effective type. Checked length
///          scaling proves that a wide-character descriptor cannot wrap while changing units from elements to bytes.
template <::std::integral char_type>
inline constexpr void scatter_materialize_byte_descriptors(
	::fast_io::io_scatter_t *destination,
	::fast_io::basic_io_scatter_t<char_type> const *source, ::std::size_t count) noexcept
{
	for (::std::size_t i{}; i != count; ++i)
	{
		destination[i] = {
			source[i].base,
			::fast_io::details::intrinsics::mul_or_overflow_die(source[i].len, sizeof(char_type))};
	}
}

/// @brief Performs the one-byte typed-to-byte scatter `some` adaptation outside the caller's hot frame.
/// @details GCC 15 otherwise reserved the full one-KiB workspace in fitting buffered callers. A semantically marked
///          native observer is transported by value; every other observer stays borrowed. One conditional signature
///          therefore preserves both the measured placement and the observer's required identity without a clone.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr ::fast_io::io_scatter_status_t scatter_write_some_via_byte_descriptors_cold_impl(
	::fast_io::details::scatter_write_some_byte_adapter_stream_t<outstmtype> outsm,
	::fast_io::basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	static_assert(sizeof(char_type) == 1u);
	constexpr ::std::size_t capacity{
		::fast_io::details::scatter_byte_conversion_stack_capacity};
	::std::size_t const count{n < capacity ? n : capacity};
	::fast_io::io_scatter_t converted[capacity];
	::fast_io::details::scatter_materialize_byte_descriptors(
		converted, pscatters, count);
	if constexpr (::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype>)
	{
		return ::fast_io::details::scatter_write_some_bytes_abi_value_direct_impl(outsm, converted, count);
	}
	else
	{
		return ::fast_io::details::scatter_write_some_bytes_cold_impl(outsm, converted, count);
	}
}

/// @brief Performs typed-to-byte scatter `all` adaptation outside the caller's hot frame.
/// @details Every chunk preserves descriptor and byte order. The conditional stream parameter transports only observers
///          whose native-scatter and scalar-completion leaves both carry explicit semantic value proofs.
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scatter_write_all_via_byte_descriptors_cold_impl(
	::fast_io::details::scatter_write_all_byte_adapter_stream_t<outstmtype> outsm,
	::fast_io::basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
	::std::size_t n)
{
	constexpr ::std::size_t capacity{
		::fast_io::details::scatter_byte_conversion_stack_capacity};
	::fast_io::io_scatter_t converted[capacity];
	while (n != 0u)
	{
		::std::size_t const count{n < capacity ? n : capacity};
		::fast_io::details::scatter_materialize_byte_descriptors(
			converted, pscatters, count);
		if constexpr (
			::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype> &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			::fast_io::details::scatter_write_all_bytes_abi_value_direct_impl(outsm, converted, count);
		}
		else
		{
			::fast_io::details::scatter_write_all_bytes_cold_impl(outsm, converted, count);
		}
		pscatters += count;
		n -= count;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_write_some_cold_impl(
	outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters, ::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		// A some-operation submits one legal backend request and reports progress within that submitted prefix. It must
		// not silently process a second batch: callers use a short status to decide whether and how to retry.
		::std::size_t const count{
			::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
		return scatter_write_some_overflow_define(outsm, pscatters, count);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		// Every descriptor preceding i has been consumed completely. Consequently the first short scalar write maps
		// directly to {i, sz}, preserving the scatter prefix contract without reconstructing a byte count.
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [base, len] = pscatters[i];
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			auto written{::fast_io::details::write_some_impl(outsm, range.first, range.last)};
			::std::size_t sz{static_cast<::std::size_t>(written - range.first)};
			if (sz != len)
			{
				return {i, sz};
			}
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		scatter_write_all_cold_impl(outsm, pscatters, n);
		return {n, 0};
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		if constexpr (sizeof(char_type) == 1u)
		{
			// A some-operation is allowed to complete only a prefix. Materializing one bounded prefix therefore preserves
			// its progress contract and still gives a native byte-scatter backend one combined request. In contrast to the
			// former reinterpret-read, every descriptor in `converted` is a real `io_scatter_t` object. Element and byte
			// progress are identical only in this one-byte branch.
			return ::fast_io::details::scatter_write_some_via_byte_descriptors_cold_impl<outstmtype>(
				outsm, pscatters, n);
		}
		else
		{
			// A byte some-operation reports partial progress in bytes, while the typed contract reports complete elements.
			// Scalarizing wide descriptors lets this adapter finish a partially emitted element before returning its typed
			// successor; forwarding a byte scatter status directly would expose the wrong unit and could split an element.
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basef, len] = pscatters[i];
				auto const range{::fast_io::details::scatter_to_scalar_range(basef, len)};
				::std::byte const *base{reinterpret_cast<::std::byte const *>(range.first)};
				::std::byte const *ed{reinterpret_cast<::std::byte const *>(range.last)};
				auto written{::fast_io::details::write_some_bytes_impl(outsm, base, ed)};
				::std::size_t diff{static_cast<::std::size_t>(written - base)};
				::std::size_t md{diff % sizeof(char_type)};
				::std::size_t sz{diff / sizeof(char_type)};
				if (md != 0u)
				{
					::std::size_t dfd{sizeof(char_type) - md};
					::fast_io::details::write_all_bytes_impl(outsm, written, written + dfd);
					++sz;
				}
				if (sz != len)
				{
					return {i, sz};
				}
			}
			return {n, 0u};
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{scatter_pwrite_some_cold_impl(outsm, pscatters, n, current_position)};
		// Seek capability supplies the logical character origin, while the positional concepts prove that the matching
		// typed request does not consume that origin. The scatter status contract denotes a prefix of `pscatters`; hence
		// `fposoffadd_scatters` is the exact sequential delta and the final relative seek publishes precisely that prefix.
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, fposoffadd_scatters(0, pscatters, ret),
															   ::fast_io::seekdir::cur);
		return ret;
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
		auto ret{::fast_io::details::scatter_pwrite_some_typed_at_byte_offset_cold_impl(
			outsm, pscatters, n, current_position)};
		// Both the seek and positional primitive operate in bytes. Keeping the queried origin in that unit avoids an
		// unjustified divisibility assumption for wide characters; the helper completes partial character objects before
		// returning typed progress, so checked scatter accumulation yields the exact byte delta published here.
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, ret)),
			::fast_io::seekdir::cur);
		return ret;
	}
	else
	{
		// A finite put-area-only stream has no backend on which the cold suffix can make progress.  The hot caller
		// adds its already-buffered descriptor prefix to this zero status, which is exactly the `some` contract; an
		// all-operation retains its separate overflow/termination policy.
		return {};
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t
scatter_write_some_impl(outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
						::std::size_t n)
{
	if (n == 0u)
	{
		// The empty prefix is already complete. Returning before mutex, buffer, and device dispatch makes the
		// descriptor-limit contract literal: native scatter calls are always nonempty.
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			// Character preservation keeps every typed descriptor valid after unwrapping; strict type progress prevents
			// recursive relocking. Thus all descriptors and fallback chunks share one guard for this logical operation.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_write_some_impl(unlocked, pscatters, n);
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

		// A lazy put area contributes zero capacity until its overflow provider
		// allocates storage; the shared helper avoids subtracting its null sentinels.
		::std::size_t buffptrdiff{
			::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};

		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [base, len] = *i;
			// `[curr, ed)` has `buffptrdiff` writable elements.  Equality consumes the final element and is a
			// completed buffered scatter, not an overflow condition.
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				curr = ::fast_io::details::non_overlapped_copy_n(base, len, curr);
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
			auto ret{::fast_io::details::scatter_write_some_cold_impl(outsm, i, static_cast<::std::size_t>(e - i))};
			ret.position += static_cast<::std::size_t>(i - pscatters);
			return ret;
		}
		return {n, 0};
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_write_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::scatter_write_some_via_byte_descriptors_cold_impl<outstmtype>(
				outsm, pscatters, n);
		}
		return scatter_write_some_cold_impl(outsm, pscatters, n);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_write_all_cold_impl(outstmtype &outsm,
							basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters, ::std::size_t n)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, outstmtype>()};
		// Partition [0,n) into consecutive batches of at most maximum descriptors. Each overflow all-operation consumes
		// its entire batch, so advancing by maximum preserves order and proves that the concatenation of the batches is
		// exactly the original output. SIZE_MAX is the unlimited sentinel and makes the loop condition false. Otherwise
		// the concept requires maximum != 0, hence n decreases on every iteration and termination is guaranteed.
		while (maximum < n)
		{
			scatter_write_all_overflow_define(outsm, pscatters, maximum);
			pscatters += maximum;
			n -= maximum;
		}
		scatter_write_all_overflow_define(outsm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::write_all_impl(outsm, range.first, range.last);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_write_some_impl(outsm, pscatters, n)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			::std::size_t pisc{ret.position_in_scatter};
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::fast_io::details::write_all_impl(outsm, pi.base + pisc, pi.base + pi.len);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::write_all_impl(outsm, range.first, range.last);
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		// All-output semantics permit consecutive batches because each byte cold call consumes its complete prefix.
		// Chunking bounds stack use while preserving descriptor and byte order for arbitrarily large typed plans.
		::fast_io::details::scatter_write_all_via_byte_descriptors_cold_impl<outstmtype>(
			outsm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		scatter_pwrite_all_cold_impl(outsm, pscatters, n, current_position);
		// A positional all-operation consumes the complete descriptor range without changing current position. The seek
		// result and typed positional offset share character units; advancing by the checked total therefore reproduces
		// one ordinary sequential scatter write at the queried origin.
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
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
		::fast_io::details::scatter_pwrite_all_typed_at_byte_offset_cold_impl(
			outsm, pscatters, n, current_position);
		// The byte-offset helper preserves an arbitrary byte origin and consumes every typed descriptor completely. Since
		// positional I/O leaves current position unchanged, the only state publication required is the complete typed
		// extent converted once to bytes by the saturating helper.
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm,
			::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, {n, 0})),
			::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
inline constexpr void scatter_write_all_impl(outstmtype &outsm,
											 basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
											 ::std::size_t n)
{
	if (n == 0u)
	{
		// No lock, buffer flush, seek synthesis, or native zero-iovec operation is observable for an empty list.
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_write_all_impl(unlocked, pscatters, n);
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

		// Scatter-all observes the same lazy-zero/live-array put-area invariant as
		// scalar output and must not form a null pointer difference before fallback.
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
				curr = ::fast_io::details::non_overlapped_copy_n(base, len, curr);
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
			return ::fast_io::details::scatter_write_all_cold_impl(outsm, i, static_cast<::std::size_t>(e - i));
		}
	}
	else if constexpr (
		::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		/*
		A native all-scatter CPO already owns completion of one legal request.
		Keep the overwhelmingly common fitting request in the caller so a constant
		descriptor count folds directly to the backend call.  The former unconditional
		cold hop made GCC 11/12/15/16 retain an extra call even when `n` and the
		backend limit were compile-time constants.  Oversized requests still enter the
		existing consecutive-batch loop, so this placement change cannot alter the
		maximum-count contract or descriptor order.
		*/
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<
				typename outstmtype::output_char_type, outstmtype>()};
		if (n <= maximum)
		{
			return scatter_write_all_overflow_define(outsm, pscatters, n);
		}
		return ::fast_io::details::scatter_write_all_cold_impl(
			outsm, pscatters, n);
	}
	else
	{
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_write_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_scatter_write_some_bytes<outstmtype> &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::scatter_write_all_via_byte_descriptors_cold_impl<outstmtype>(
				outsm, pscatters, n);
		}
		return ::fast_io::details::scatter_write_all_cold_impl(outsm, pscatters, n);
	}
}

} // namespace details

} // namespace fast_io

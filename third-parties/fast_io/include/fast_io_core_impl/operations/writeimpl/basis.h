#pragma once

/*
 * Sequential scalar output synthesis (primitive operation sublayer).
 *
 * Starting from the exact typed/byte write and obuffer CPOs advertised by a
 * normalized output observer, this file derives contiguous `write_some` and
 * `write_all` behavior, including retry/progress and overflow paths. It moves
 * fully materialized character or byte ranges and knows nothing about printable
 * objects, semantic records, format syntax, or output scenarios.
 * Temporary seek/flush fallbacks use the named-observer dispatch bridge, which
 * preserves inline cursor identity unless both semantic substitution and target
 * ABI proofs explicitly permit value transport.
 */

namespace fast_io
{
namespace details
{

template <typename element_type>
inline constexpr element_type scatter_scalar_empty_anchor{};

template <typename element_type>
struct basic_scatter_scalar_range
{
	element_type const *first;
	element_type const *last;
};

/**
 * @brief Converts one scatter descriptor to the pointer pair required by a scalar output primitive.
 *
 * @details A zero-length scatter is allowed to carry a null base because it denotes no readable object.  C++ pointer
 *          arithmetic and subtraction, however, are defined only within an array object; consequently `base + 0` and
 *          a later `last - first` are not valid when both pointers are null.  A null empty descriptor is therefore
 *          represented by two pointers to a stable inline anchor.  A non-null empty base is deliberately preserved so
 *          observers that distinguish descriptor provenance keep seeing the original address.  Positive extents retain
 *          the normal scatter precondition that `base` begins a valid array range.
 *
 *          This normalization changes neither the byte sequence nor the strategy's exception boundary: callers retain
 *          their existing one scalar-range dispatch per descriptor, including an empty descriptor, and only the
 *          otherwise-invalid null pointer representation is replaced.
 *
 *          The empty arm is marked unlikely for optimizer cost modelling, not as a semantic precondition.  Apple Clang
 *          otherwise stops inlining a cold scatter loop even when every descriptor length is compile-time nonzero;
 *          the annotation restores instruction-for-instruction baseline code after constant propagation.  GCC keeps
 *          the same generic cold-loop layout with the annotation, while run-time empty descriptors remain supported.
 */
template <typename element_type>
inline constexpr basic_scatter_scalar_range<element_type>
scatter_to_scalar_range(element_type const *base, ::std::size_t len) noexcept
{
	if (len == 0u) [[unlikely]]
	{
		if (base == nullptr)
		{
			base = __builtin_addressof(scatter_scalar_empty_anchor<element_type>);
		}
		return {base, base};
	}
	return {base, base + len};
}

/**
 * @brief Computes a scalar range distance without subtracting an empty null pair.
 *
 * @details Equality is a complete zero-length proof and is tested before pointer
 *          arithmetic because a canonical empty range may be `{nullptr,nullptr}`.
 *          A nonempty range retains the ordinary precondition that both endpoints
 *          belong to one ordered array. This helper preserves calls made for empty
 *          ranges; it changes only the internal representation of their length.
 */
template <typename element_type>
inline constexpr ::std::ptrdiff_t output_pointer_distance(
	element_type const *first, element_type const *last) noexcept
{
	return first == last ? 0 : last - first;
}

template <typename element_type>
inline constexpr ::std::size_t output_pointer_range_size(
	element_type const *first, element_type const *last) noexcept
{
	return static_cast<::std::size_t>(
		::fast_io::details::output_pointer_distance(first, last));
}

/**
 * @brief Advances a scalar cursor without forming `nullptr + 0`.
 *
 * @details A zero progress result denotes the unchanged cursor and therefore has
 *          no pointer-domain precondition. Positive progress retains the scalar
 *          primitive contract that the source begins a live array range.
 */
template <typename element_type>
inline constexpr element_type const *output_pointer_advance(
	element_type const *first, ::std::size_t count) noexcept
{
	return count == 0u ? first : first + count;
}

/**
 * @brief Computes writable put-area capacity without subtracting lazy null sentinels.
 *
 * @details The basic obuffer protocol permits implementations whose unallocated
 *          state is `{nullptr,nullptr,nullptr}`. Pointer subtraction is not
 *          defined for that state, even though its mathematical capacity is
 *          zero. Audited providers carrying `obuffer_address_distance_safe`
 *          additionally prove that a live pair belongs to one ordered array;
 *          their run-time address difference is therefore representation-safe
 *          and also maps the all-null pair to zero. Other providers receive an
 *          explicit null check before ordinary same-array subtraction. A
 *          negative live difference is a malformed put area and is treated as
 *          zero so primitive dispatch takes its ordinary overflow path without
 *          manufacturing a huge unsigned capacity.
 */
template <::std::integral char_type, typename output_type>
inline constexpr ::std::size_t obuffer_remaining_size(
	char_type *current, char_type *end) noexcept
{
	if constexpr (::fast_io::obuffer_address_distance_safe<char_type, output_type>)
	{
		if (::std::is_constant_evaluated())
		{
			return current == nullptr || end == nullptr
					   ? 0u
					   : static_cast<::std::size_t>(end - current);
		}
		auto const current_address{reinterpret_cast<::std::uintptr_t>(current)};
		auto const end_address{reinterpret_cast<::std::uintptr_t>(end)};
		return static_cast<::std::size_t>(
			(end_address - current_address) / sizeof(char_type));
	}
	else
	{
		if (current == nullptr || end == nullptr) [[unlikely]]
		{
			return 0u;
		}
		auto const difference{end - current};
		return difference < 0 ? 0u : static_cast<::std::size_t>(difference);
	}
}

template <typename outstmtype>
inline constexpr typename outstmtype::output_char_type const *
pwrite_some_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
					  typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t);

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
																::std::byte const *last, ::fast_io::intfpos_t off);

template <typename outstmtype>
inline constexpr void pwrite_all_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
										   typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off);

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last,
												 ::fast_io::intfpos_t);

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
															   ::std::byte const *last);

template <typename outstmtype>
inline constexpr void write_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last);

/**
 * @brief Recognizes a copy-substitutable observer whose scalar byte-write leaf accepts a value expression.
 * @details The ADL stream marker proves identity-independent state transition;
 *          the explicit prvalue invocation proves that the leaf is reachable
 *          without a mutable-reference-only provider. This conjunction, plus
 *          the target ABI policy embedded in `abi_value_output_stream_ref_result`,
 *          is the complete admission rule for the direct path.
 */
template <typename outstmtype>
concept abi_value_direct_write_some_bytes =
	::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &> &&
	requires(outstmtype &outsm, ::std::byte const *ptr) {
		{
			write_some_bytes_overflow_define(outstmtype{outsm}, ptr, ptr)
		} -> ::std::same_as<::std::byte const *>;
	};

template <typename outstmtype>
	requires ::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte const *
write_some_bytes_abi_value_direct_impl(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last)
{
	return write_some_bytes_overflow_define(outstmtype{outsm}, first, last);
}

/**
 * @brief Repeats the direct value leaf until the complete byte range is written.
 * @details This is the same monotone prefix recurrence as the ordinary cold
 *          synthesis branch. It changes only observer transport: every leaf
 *          receives a register-eligible copy proven to denote the same external
 *          stream state, while unmarked observers never instantiate this path.
 */
template <typename outstmtype>
	requires ::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
write_all_bytes_abi_value_direct_impl(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last)
{
	while ((first = write_some_bytes_overflow_define(
			outstmtype{outsm}, first, last)) != last)
	{}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr typename outstmtype::output_char_type const *
write_some_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
					 typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		return write_some_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		write_all_overflow_define(outsm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		// Scatter `all` returns void: successful return proves that its sole descriptor was consumed in full.
		basic_io_scatter_t<char_type> sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
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
				write_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last))};
			// One-byte character progress is address-identical to byte progress; casting
			// the returned cursor back avoids both subtraction and `nullptr + 0`.
			return reinterpret_cast<char_type_const_ptr>(ptr);
		}
		else
		{
			::std::byte const *firstptr{reinterpret_cast<::std::byte const *>(first)};
			::std::byte const *ptr{
				write_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last))};
			::std::size_t const diff{
				::fast_io::details::output_pointer_range_size(firstptr, ptr)};
			::std::size_t v{diff / sizeof(char_type)};
			::std::size_t const partial{diff % sizeof(char_type)};
			if (partial != 0)
			{
				// A typed some-operation may report only complete elements. Complete the partially emitted element before
				// returning its successor; writing `partial` bytes here would leave the element truncated unless its size
				// happened to be exactly twice the observed prefix.
				write_all_bytes_cold_impl(outsm, ptr, ptr + (sizeof(char_type) - partial));
				++v;
			}
			return ::fast_io::details::output_pointer_advance(first, v);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto current_position{::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pwrite_some_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm,
			::fast_io::details::output_pointer_distance(first, ret) + current_position,
			::fast_io::seekdir::beg);
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
		::std::byte const *const first_bytes{reinterpret_cast<::std::byte const *>(first)};
		::std::byte const *const last_bytes{reinterpret_cast<::std::byte const *>(last)};
		::std::byte const *written{
			::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first_bytes, last_bytes, current_position)};
		::std::ptrdiff_t const byte_difference{
			::fast_io::details::output_pointer_distance(first_bytes, written)};
		::std::size_t const complete_characters{
			static_cast<::std::size_t>(byte_difference) / sizeof(char_type)};
		::std::size_t const partial_bytes{
			static_cast<::std::size_t>(byte_difference) % sizeof(char_type)};
		::std::size_t character_progress{complete_characters};
		if (partial_bytes != 0u)
		{
			::std::size_t const remaining_bytes{sizeof(char_type) - partial_bytes};
			auto const continuation_position{
				::fast_io::fposoffadd_nonegative(current_position, byte_difference)};
			::fast_io::details::pwrite_all_bytes_cold_impl(
				outsm, written, written + remaining_bytes, continuation_position);
			++character_progress;
		}
		// The selected concepts prove an exact byte seek result and at least one byte-positional output primitive.
		// The primitive progress contract keeps `written` in [first_bytes,last_bytes]. Passing the queried byte
		// position directly therefore preserves an unaligned current position; rounding through a character offset
		// would target preceding storage. Completing a partial character makes `character_progress` a typed cursor,
		// and the checked conversion below advances the sequential position by exactly that emitted prefix.
		auto const byte_progress{::fast_io::details::scatter_fpos_mul<char_type>(
			::fast_io::fposoffadd_nonegative(0, character_progress))};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, byte_progress, ::fast_io::seekdir::cur);
		return ::fast_io::details::output_pointer_advance(first, character_progress);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::std::byte const *write_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
															   ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		return write_some_bytes_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		::std::size_t const len{
			::fast_io::details::output_pointer_range_size(first, last)};
		io_scatter_t sc{first, len};
		auto const progress{::fast_io::scatter_status_one_size(
			scatter_write_some_bytes_overflow_define(outsm, __builtin_addressof(sc), 1), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		auto const result{write_some_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
													 reinterpret_cast<char_type_const_ptr>(last))};
		return reinterpret_cast<::std::byte const *>(result);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
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
			scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len)};
		return ::fast_io::details::output_pointer_advance(first, progress);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		write_all_bytes_overflow_define(outsm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_write_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		write_all_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
								  reinterpret_cast<char_type_const_ptr>(last));
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		basic_io_scatter_t<char_type> sc{
			reinterpret_cast<char_type_const_ptr>(first),
			::fast_io::details::output_pointer_range_size(first, last)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
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
		auto ret{::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(first, ret),
			::fast_io::seekdir::cur);
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
		auto ret{::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(first, ret),
			::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void write_all_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
										  typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		write_all_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		basic_io_scatter_t<char_type> sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			while ((first = write_some_overflow_define(outsm, first, last)) != last)
			{
				char_type *curr{obuffer_curr(outsm)};
				char_type *ed{obuffer_end(outsm)};
				::std::size_t const bfddiff{
					::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::output_pointer_range_size(first, last)};
				// The put area is the half-open writable range `[curr,ed)`: equality consumes that complete range and may
				// publish `ed` as the next cursor. Re-entering the native some-operation on equality can flush or grow a
				// destination even though the remaining input already fits.
				if (itdiff <= bfddiff)
				{
					obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
					return;
				}
			}
		}
		else
		{
			while ((first = write_some_overflow_define(outsm, first, last)) != last)
				;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		// The one-descriptor adapter must consume progress returned by the typed scatter customization. It cannot call
		// the byte customization (which the stream need not provide), and unlike an `all` primitive the result is not a
		// completion proof, so retry until the descriptor is complete.
		while (first != last)
		{
			::std::size_t const len{
				::fast_io::details::output_pointer_range_size(first, last)};
			basic_io_scatter_t<char_type> sc{first, len};
			first += ::fast_io::scatter_status_one_size(
				scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len);
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		write_all_bytes_cold_impl(outsm, reinterpret_cast<::std::byte const *>(first),
								  reinterpret_cast<::std::byte const *>(last));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_cold_impl(outsm, first, last, current_position);
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(first, last),
			::fast_io::seekdir::cur);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto firstbptr{reinterpret_cast<::std::byte const *>(first)};
		auto lastbptr{reinterpret_cast<::std::byte const *>(last)};
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_bytes_cold_impl(outsm, firstbptr, lastbptr, current_position);
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(firstbptr, lastbptr),
			::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void write_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		write_all_bytes_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::output_pointer_range_size(first, last)};
		scatter_write_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			while ((first = write_some_bytes_overflow_define(outsm, first, last)) != last)
			{
				char_type *curr{obuffer_curr(outsm)};
				char_type *ed{obuffer_end(outsm)};
				::std::size_t const bfddiff{
					::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::output_pointer_range_size(first, last)};
				// Byte and typed completion share the same closed cursor contract. An exact remainder is complete in this
				// put area and must not make a second native call merely to leave one element unused.
				if (itdiff <= bfddiff)
				{
					obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
					return;
				}
			}
		}
		else
		{
			while ((first = write_some_bytes_overflow_define(outsm, first, last)) != last)
				;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		while (first != last)
		{
			::std::size_t const len{
				::fast_io::details::output_pointer_range_size(first, last)};
			io_scatter_t sc{first, len};
			first += ::fast_io::scatter_status_one_size(
				scatter_write_some_bytes_overflow_define(outsm, __builtin_addressof(sc), 1), len);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>))
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		char_type_const_ptr firstcptr{reinterpret_cast<char_type_const_ptr>(first)};
		char_type_const_ptr lastcptr{reinterpret_cast<char_type_const_ptr>(last)};
		::fast_io::details::write_all_cold_impl(outsm, firstcptr, lastcptr);
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
		::fast_io::details::pwrite_all_bytes_cold_impl(outsm, first, last, current_position);
		::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(first, last),
			::fast_io::seekdir::cur);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		char_type_const_ptr firstcptr{reinterpret_cast<char_type_const_ptr>(first)};
		char_type_const_ptr lastcptr{reinterpret_cast<char_type_const_ptr>(last)};
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay_dispatch(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_cold_impl(outsm, firstcptr, lastcptr, current_position);
		::fast_io::operations::decay::output_stream_seek_decay_dispatch(
			outsm, ::fast_io::details::output_pointer_distance(firstcptr, lastcptr),
			::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
inline constexpr typename outstmtype::output_char_type const *
write_some_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
				typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			// The complete protocol proves guard storage, character identity, and strict type progress. Keeping the
			// recursive primitive inside this guard makes all fallback retries part of one logical, atomic write.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// Name the unlocked result exactly once. `decltype(auto)` owns a prvalue proxy for this call but preserves an
			// lvalue result as a reference, so recursive fallback neither dangles nor introduces an ABI-visible copy.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_some_impl(unlocked, first, last);
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
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::size_t const bfddiff{
				::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::output_pointer_range_size(first, last)};
			// Publishing the one-past cursor is part of the basic put-area contract, so an exact fit is a successful
			// buffered write rather than an overflow operation.
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
				return last;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_write_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			auto first_bytes{reinterpret_cast<::std::byte const *>(first)};
			auto result{::fast_io::details::write_some_bytes_abi_value_direct_impl(
				outsm, first_bytes, reinterpret_cast<::std::byte const *>(last))};
			// The byte leaf returns an address in the original one-byte character
			// range. Reinterpreting that cursor preserves zero progress without null
			// subtraction or null pointer arithmetic.
			return reinterpret_cast<char_type const *>(result);
		}
		return ::fast_io::details::write_some_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
									 typename outstmtype::output_char_type const *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_all_impl(unlocked, first, last);
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
		using char_type = typename outstmtype::output_char_type;
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::size_t const bfddiff{
				::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::output_pointer_range_size(first, last)};
			// `[curr,ed)` contains exactly `bfddiff` writable elements. Equality therefore completes the request and
			// may publish `ed`; sending it to an overflow CPO would add a spurious flush or geometric growth.
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
				return;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_write_operations<outstmtype> &&
			sizeof(typename outstmtype::output_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::write_all_bytes_abi_value_direct_impl(
				outsm, reinterpret_cast<::std::byte const *>(first),
				reinterpret_cast<::std::byte const *>(last));
		}
		::fast_io::details::write_all_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_impl(outstmtype &outsm, ::std::byte const *first,
														  ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_some_bytes_impl(unlocked, first, last);
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
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::size_t const bfddiff{
				::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::output_pointer_range_size(first, last)};
			// For a one-byte output character, equality consumes the complete live put area and yields its valid
			// one-past cursor; it does not require byte-overflow synthesis.
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				obuffer_set_curr(outsm, non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(first),
															  static_cast<::std::size_t>(itdiff), curr));
				return last;
			}
		}
		if constexpr (::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::write_some_bytes_abi_value_direct_impl(
				outsm, first, last);
		}
		return ::fast_io::details::write_some_bytes_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_bytes_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_all_bytes_impl(unlocked, first, last);
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
		using char_type = typename outstmtype::output_char_type;
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::size_t const bfddiff{
				::fast_io::details::obuffer_remaining_size<char_type, outstmtype>(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::output_pointer_range_size(first, last)};
			// Exact byte fits are ordinary put-area completions. Keeping them hot prevents a growable adapter from
			// doubling capacity after the final useful byte has already been copied.
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				obuffer_set_curr(outsm, non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(first),
															  static_cast<::std::size_t>(itdiff), curr));
				return;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> &&
			::fast_io::details::abi_value_direct_write_some_bytes<outstmtype>)
		{
			return ::fast_io::details::write_all_bytes_abi_value_direct_impl(
				outsm, first, last);
		}
		::fast_io::details::write_all_bytes_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
char_put_cold_impl(outstmtype &outstm,
				   typename outstmtype::output_char_type ch)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_stream_char_put_overflow_define<outstmtype>)
	{
		output_stream_char_put_overflow_define(outstm, ch);
	}
	else
	{
		::fast_io::details::write_all_impl(outstm, __builtin_addressof(ch), __builtin_addressof(ch) + 1);
	}
}

template <typename outstm>
inline constexpr void
char_put_impl(outstm &outsm, typename outstm::output_char_type ch)
{
	// `outsm` is already a normalized stream reference.  Reading the character type directly is essential after mutex
	// unwrapping: the unlocked reference is an internal leaf protocol and need not advertise another public
	// `output_stream_ref_define`.  Re-normalizing it here made the otherwise complete mutex protocol ill-formed.
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstm>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstm>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::char_put_impl(unlocked, ch);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstm>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstm>)
		{
			using char_type = typename outstm::output_char_type;
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			bool condition;
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_is_line_buffering_define<outstm>)
			{
				condition = curr < ed;
			}
			else
			{
				condition = curr != ed;
			}
			if (condition)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif

			{
				*curr = ch;
				obuffer_set_curr(outsm, curr + 1);
				return;
			}
		}
		::fast_io::details::char_put_cold_impl(outsm, ch);
	}
}

} // namespace details

} // namespace fast_io

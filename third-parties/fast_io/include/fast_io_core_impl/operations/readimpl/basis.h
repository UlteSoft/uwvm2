#pragma once

/*
 * Sequential scalar input synthesis (primitive operation sublayer).
 *
 * Starting from the exact typed/byte read and ibuffer CPOs advertised by a
 * normalized input observer, this file derives contiguous `read_some` and
 * `read_all` behavior, including refill, retry, and progress handling. It fills
 * raw character or byte ranges and does not interpret scanned target objects or
 * format syntax.
 */

namespace fast_io
{
namespace details
{

template <typename element_type>
inline element_type input_scatter_scalar_empty_anchor{};

template <typename element_type>
struct basic_input_scatter_scalar_range
{
	element_type *first;
	element_type *last;
};

/**
 * @brief Forms a scalar destination range from one scatter descriptor without null pointer arithmetic.
 * @details A zero-length input descriptor denotes no writable object and may
 *          therefore carry a null base. C++ pointer arithmetic is nevertheless
 *          defined only within an array object, so `base + 0` is not a valid way
 *          to form its scalar endpoint. The scalar fallback substitutes a stable
 *          mutable-typed anchor only for the null-empty representation. The
 *          half-open range remains empty, so the scalar CPO contract grants no
 *          permission to dereference or modify the anchor. This preserves one
 *          scalar CPO invocation per descriptor, while a non-null empty base
 *          retains its address and every positive extent retains the ordinary
 *          writable-array precondition.
 */
template <typename element_type>
inline constexpr basic_input_scatter_scalar_range<element_type>
scatter_to_input_scalar_range(element_type *base, ::std::size_t len) noexcept
{
	if (len == 0u) [[unlikely]]
	{
		if (base == nullptr)
		{
			base = __builtin_addressof(
				::fast_io::details::input_scatter_scalar_empty_anchor<element_type>);
		}
		return {base, base};
	}
	return {base, base + len};
}

/**
 * @brief Computes input progress without subtracting an empty null pointer pair.
 * @details Equality is a complete zero-length proof and precedes subtraction
 *          because public scalar input permits `{nullptr,nullptr}` to represent
 *          an empty destination. A nonempty range retains the primitive CPO
 *          invariant that both pointers belong to one ordered writable array.
 */
template <typename element_type>
inline constexpr ::std::ptrdiff_t input_pointer_distance(
	element_type const *first, element_type const *last) noexcept
{
	return first == last ? 0 : last - first;
}

template <typename element_type>
inline constexpr ::std::size_t input_pointer_range_size(
	element_type const *first, element_type const *last) noexcept
{
	return static_cast<::std::size_t>(
		::fast_io::details::input_pointer_distance(first, last));
}

/**
 * @brief Advances an input cursor only when progress is positive.
 * @details Zero progress returns the original cursor without evaluating
 *          `nullptr + 0`. Positive progress retains the provider's prefix proof
 *          that the cursor starts a live destination or input-buffer array.
 */
template <typename element_type>
inline constexpr element_type *input_pointer_advance(
	element_type *first, ::std::size_t count) noexcept
{
	return count == 0u ? first : first + count;
}

/**
 * @brief Computes readable get-area capacity for both live and lazy-null states.
 * @details The ibuffer protocol exposes either an ordered pair in one live
 *          character array or an equal empty pair; `basic_io_buffer` uses two
 *          null sentinels before its first refill. Testing equality first maps
 *          both empty representations to zero without pointer subtraction. The
 *          remaining branch is governed by the live same-array protocol, and a
 *          malformed negative extent is conservatively treated as empty.
 */
template <typename element_type>
inline constexpr ::std::size_t ibuffer_remaining_size(
	element_type const *current, element_type const *end) noexcept
{
	if (current == end)
	{
		return 0u;
	}
	auto const difference{end - current};
	return difference < 0 ? 0u : static_cast<::std::size_t>(difference);
}

// Details-layer ownership contract: the caller owns (or stably borrows) one normalized input observer and every
// scalar fallback receives that identical object by lvalue reference. A read may cross buffered, byte, positional,
// and scatter strategies; passing the observer by value at each edge would multiply proxy construction and make a
// valid move-only stream-ref unusable. The reference does not prescribe a calling convention: after inlining, an
// ABI-aware public boundary may still keep a proven small trivial observer in registers, while unknown aggregate ABIs
// and nontrivial observers retain identity without relying on an unsafe universal size rule.
// Seek-based synthesis therefore calls the named-observer dispatch bridge: it may recover value ABI only after the
// same explicit semantic and target proof, never merely because this implementation happens to hold an lvalue.

template <typename instmtype>
inline constexpr typename instmtype::input_char_type *
pread_some_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
					 typename instmtype::input_char_type *last, ::fast_io::intfpos_t);

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
														 ::fast_io::intfpos_t off);

template <typename instmtype>
inline constexpr void pread_all_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
										  typename instmtype::input_char_type *last, ::fast_io::intfpos_t off);

template <typename instmtype>
inline constexpr void pread_all_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last,
												::fast_io::intfpos_t);

template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last);

template <typename instmtype>
inline constexpr void read_all_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last);

/**
 * @brief Recognizes a semantically substitutable observer whose scalar byte-read leaf accepts a value expression.
 * @details The stream marker proves that replacing the normalized observer by a
 *          copy preserves the external state transition. The prvalue call below
 *          separately proves that the selected provider does not require a
 *          mutable observer identity. Both obligations are required before an
 *          outlined cold `T&` synthesis edge may be bypassed.
 */
template <typename instmtype>
concept abi_value_direct_read_some_bytes =
	::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &> &&
	requires(instmtype &insm, ::std::byte *ptr) {
		{
			read_some_bytes_underflow_define(instmtype{insm}, ptr, ptr)
		} -> ::std::same_as<::std::byte *>;
	};

template <typename instmtype>
	requires ::fast_io::details::abi_value_direct_read_some_bytes<instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte *
read_some_bytes_abi_value_direct_impl(
	instmtype insm, ::std::byte *first, ::std::byte *last)
{
	return read_some_bytes_underflow_define(instmtype{insm}, first, last);
}

/**
 * @brief Repeats the direct value leaf while preserving the ordinary all-read progress invariant.
 * @details For positions `p_i`, each successful leaf establishes
 *          `p_i <= p_(i+1) <= last`; equality before `last` is EOF, and reaching
 *          `last` proves complete initialization. Fresh observer copies are
 *          admissible only because `abi_value_direct_read_some_bytes` includes
 *          the explicit shared-state substitution proof.
 */
template <typename instmtype>
	requires ::fast_io::details::abi_value_direct_read_some_bytes<instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
read_all_bytes_abi_value_direct_impl(
	instmtype insm, ::std::byte *first, ::std::byte *last)
{
	for (;;)
	{
		auto next{read_some_bytes_underflow_define(
			instmtype{insm}, first, last)};
		if (next == last)
		{
			return;
		}
		if (next == first)
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
		}
		first = next;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr
	typename instmtype::input_char_type *read_some_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
															 typename instmtype::input_char_type *last)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		return read_some_underflow_define(insm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		auto [pos, scpos]{scatter_read_some_underflow_define(insm, __builtin_addressof(sc), 1)};
		if (!pos)
		{
			return ::fast_io::details::input_pointer_advance(first, scpos);
		}
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		read_all_underflow_define(insm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		basic_io_scatter_t<char_type> sc{first, len};
		scatter_read_all_underflow_define(insm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>)
	{
		if constexpr (sizeof(typename instmtype::input_char_type) == 1)
		{
			using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= char_type *;
			::std::byte *firstptr{reinterpret_cast<::std::byte *>(first)};
			::std::byte *ptr{read_some_bytes_cold_impl(insm, firstptr, reinterpret_cast<::std::byte *>(last))};
			// A one-byte adaptation preserves the returned address exactly. Reinterpreting
			// that cursor also preserves null zero-progress without subtraction or addition.
			return reinterpret_cast<char_type_ptr>(ptr);
		}
		else
		{
			::std::byte *firstptr{reinterpret_cast<::std::byte *>(first)};
			::std::byte *ptr{read_some_bytes_cold_impl(insm, firstptr, reinterpret_cast<::std::byte *>(last))};
			::std::size_t const diff{
				::fast_io::details::input_pointer_range_size(firstptr, ptr)};
			::std::size_t v{diff / sizeof(char_type)};
			::std::size_t const partial{diff % sizeof(char_type)};
			if (partial != 0)
			{
				// A typed result cannot expose half of an element. Complete the suffix of the partially read element and
				// include that element in the returned progress; `partial` is the prefix size, not the missing size.
				read_all_bytes_cold_impl(insm, ptr, ptr + (sizeof(char_type) - partial));
				++v;
			}
			return ::fast_io::details::input_pointer_advance(first, v);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pread_some_cold_impl(insm, first, last, current_position)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(first, ret),
			::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::std::byte *const first_bytes{reinterpret_cast<::std::byte *>(first)};
		::std::byte *const last_bytes{reinterpret_cast<::std::byte *>(last)};
		::std::byte *read{
			::fast_io::details::pread_some_bytes_cold_impl(insm, first_bytes, last_bytes, current_position)};
		::std::ptrdiff_t const byte_difference{
			::fast_io::details::input_pointer_distance(first_bytes, read)};
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
			::fast_io::details::pread_all_bytes_cold_impl(
				insm, read, read + remaining_bytes, continuation_position);
			++character_progress;
		}
		// Byte seek and byte-pread concepts establish one coordinate system; the pread progress invariant keeps `read`
		// inside the destination range. Using the queried byte position verbatim is required when it is not character
		// aligned. Finishing a partial object before exposing typed progress preserves object bounds and lets the checked
		// multiplication advance the sequential cursor by exactly the initialized character prefix.
		auto const byte_progress{::fast_io::details::scatter_fpos_mul<char_type>(
			::fast_io::fposoffadd_nonegative(0, character_progress))};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, byte_progress, ::fast_io::seekdir::cur);
		return ::fast_io::details::input_pointer_advance(
			first, character_progress);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::std::byte *read_some_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_read_some_bytes_underflow_define<instmtype>)
	{
		return read_some_bytes_underflow_define(insm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_bytes_underflow_define<instmtype>)
	{
		::std::size_t const len{
			::fast_io::details::input_pointer_range_size(first, last)};
		io_scatter_t sc{first, len};
		auto [pos, inscpos] = scatter_read_some_bytes_underflow_define(insm, __builtin_addressof(sc), 1);
		if (!pos)
		{
			return ::fast_io::details::input_pointer_advance(first, inscpos);
		}
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype>)
	{
		read_all_bytes_underflow_define(insm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_read_all_bytes_underflow_define(insm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>))
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		return reinterpret_cast<::std::byte *>(
			read_some_cold_impl(insm, reinterpret_cast<char_type_ptr>(first), reinterpret_cast<char_type_ptr>(last)));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pread_some_bytes_cold_impl(insm, first, last, current_position)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(first, ret),
			::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pread_some_bytes_cold_impl(insm, first, last, current_position)};
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(first, ret),
			::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void read_all_cold_impl(instmtype &insm, typename instmtype::input_char_type *first,
										 typename instmtype::input_char_type *last)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		read_all_underflow_define(insm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype>)
	{
		basic_io_scatter_t<char_type> sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_read_all_underflow_define(insm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
		{
			for (decltype(first) it; (it = read_some_underflow_define(insm, first, last)) != last;)
			{
				if (it == first)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
				first = it;
				auto curr{ibuffer_curr(insm)};
				auto ed{ibuffer_end(insm)};
				::std::size_t const bfddiff{
					::fast_io::details::ibuffer_remaining_size(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::input_pointer_range_size(first, last)};
				if (itdiff <= bfddiff)
				{
					non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
					ibuffer_set_curr(
						insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
					return;
				}
			}
		}
		else
		{
			for (decltype(first) it; (it = read_some_underflow_define(insm, first, last)) != last; first = it)
			{
				if (it == first)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
			}
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
		{
			for (;;)
			{
				::std::size_t const len{
					::fast_io::details::input_pointer_range_size(first, last)};
				basic_io_scatter_t<char_type> sc{first, len};
				::std::size_t sz{::fast_io::scatter_status_one_size(
					scatter_read_some_underflow_define(insm, __builtin_addressof(sc), 1), len)};
				first = ::fast_io::details::input_pointer_advance(first, sz);
				if (first == last)
				{
					return;
				}
				if (!sz)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
				auto curr{ibuffer_curr(insm)};
				auto ed{ibuffer_end(insm)};
				::std::size_t const bfddiff{
					::fast_io::details::ibuffer_remaining_size(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::input_pointer_range_size(first, last)};
				if (itdiff <= bfddiff)
				{
					non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
					ibuffer_set_curr(
						insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
					return;
				}
			}
		}
		else
		{
			for (;;)
			{
				::std::size_t const len{
					::fast_io::details::input_pointer_range_size(first, last)};
				basic_io_scatter_t<char_type> sc{first, len};
				::std::size_t sz{::fast_io::scatter_status_one_size(
					scatter_read_some_underflow_define(insm, __builtin_addressof(sc), 1), len)};
				first = ::fast_io::details::input_pointer_advance(first, sz);
				if (first == last)
				{
					return;
				}
				if (!sz)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
			}
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		read_all_bytes_cold_impl(insm, reinterpret_cast<::std::byte *>(first), reinterpret_cast<::std::byte *>(last));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pread_all_cold_impl(insm, first, last, current_position);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(first, last),
			::fast_io::seekdir::cur);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto firstbptr{reinterpret_cast<::std::byte *>(first)};
		auto lastbptr{reinterpret_cast<::std::byte *>(last)};
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pread_all_bytes_cold_impl(insm, firstbptr, lastbptr, current_position);
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(firstbptr, lastbptr),
			::fast_io::seekdir::cur);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void read_all_bytes_cold_impl(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype>)
	{
		read_all_bytes_underflow_define(insm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype>)
	{
		io_scatter_t sc{
			first, ::fast_io::details::input_pointer_range_size(first, last)};
		scatter_read_all_bytes_underflow_define(insm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_bytes_underflow_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype> &&
					  sizeof(char_type) == 1)
		{
			for (decltype(first) it; (it = read_some_bytes_underflow_define(insm, first, last)) != last;)
			{
				if (it == first)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
				first = it;
				auto curr{ibuffer_curr(insm)};
				auto ed{ibuffer_end(insm)};
				::std::size_t const bfddiff{
					::fast_io::details::ibuffer_remaining_size(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::input_pointer_range_size(first, last)};
				if (itdiff <= bfddiff)
				{
					non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
					ibuffer_set_curr(
						insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
					return;
				}
			}
		}
		else
		{
			for (decltype(first) it; (it = read_some_bytes_underflow_define(insm, first, last)) != last; first = it)
			{
				if (it == first)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
			}
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_bytes_underflow_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
		{
			for (;;)
			{
				::std::size_t const len{
					::fast_io::details::input_pointer_range_size(first, last)};
				io_scatter_t sc{first, len};
				::std::size_t sz{::fast_io::scatter_status_one_size(
					scatter_read_some_bytes_underflow_define(insm, __builtin_addressof(sc), 1), len)};
				first = ::fast_io::details::input_pointer_advance(first, sz);
				if (first == last)
				{
					return;
				}
				if (!sz)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
				auto curr{ibuffer_curr(insm)};
				auto ed{ibuffer_end(insm)};
				::std::size_t const bfddiff{
					::fast_io::details::ibuffer_remaining_size(curr, ed)};
				::std::size_t const itdiff{
					::fast_io::details::input_pointer_range_size(first, last)};
				if (itdiff <= bfddiff)
				{
					non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
					ibuffer_set_curr(
						insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
					return;
				}
			}
		}
		else
		{
			for (;;)
			{
				::std::size_t const len{
					::fast_io::details::input_pointer_range_size(first, last)};
				io_scatter_t sc{first, len};
				::std::size_t sz{::fast_io::scatter_status_one_size(
					scatter_read_some_bytes_underflow_define(insm, __builtin_addressof(sc), 1), len)};
				first = ::fast_io::details::input_pointer_advance(first, sz);
				if (first == last)
				{
					return;
				}
				if (!sz)
				{
					::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
				}
			}
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype>)
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		char_type_ptr firstcptr{reinterpret_cast<char_type_ptr>(first)};
		char_type_ptr lastcptr{reinterpret_cast<char_type_ptr>(last)};
		::fast_io::details::read_all_cold_impl(insm, firstcptr, lastcptr);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pread_all_bytes_cold_impl(insm, first, last, current_position);
		::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(first, last),
			::fast_io::seekdir::cur);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type *;
		char_type_ptr firstcptr{reinterpret_cast<char_type_ptr>(first)};
		char_type_ptr lastcptr{reinterpret_cast<char_type_ptr>(last)};
		auto const current_position{
			::fast_io::operations::decay::input_stream_seek_decay_dispatch(insm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pread_all_cold_impl(insm, firstcptr, lastcptr, current_position);
		::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			insm, ::fast_io::details::input_pointer_distance(firstcptr, lastcptr),
			::fast_io::seekdir::cur);
	}
}

template <typename instmtype>
inline constexpr typename instmtype::input_char_type *
read_some_impl(instmtype &insm, typename instmtype::input_char_type *first, typename instmtype::input_char_type *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			// The complete protocol proves that the guard is storable, character identity is preserved, and recursive
			// dispatch makes strict type progress. Consequently this scope owns exactly one lock across every fallback
			// used to satisfy this single logical read.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			// Declare the unlocked projection after the guard. Reverse destruction then releases an owned projection while
			// the mutex is still held and unlocks only after recursive dispatch has completely returned.
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::read_some_impl(unlocked, first, last);
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
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
		{
			auto curr{ibuffer_curr(insm)};
			auto ed{ibuffer_end(insm)};
			::std::size_t const bfddiff{
				::fast_io::details::ibuffer_remaining_size(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::input_pointer_range_size(first, last)};
			// Exact fit is a successful buffered read: copying `itdiff == bfddiff` consumes the final buffered element
			// and commits the cursor to `ed`. A strict `<` would instead enter underflow despite all requested data being
			// available, changing both observable primitive-call counts and EOF behavior.
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
				ibuffer_set_curr(
					insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
				return last;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			auto first_bytes{reinterpret_cast<::std::byte *>(first)};
			auto result{::fast_io::details::read_some_bytes_abi_value_direct_impl(
				insm, first_bytes, reinterpret_cast<::std::byte *>(last))};
			// One-byte character and byte cursors have the same address. Casting the
			// returned cursor avoids null subtraction when an empty leaf reports no progress.
			return reinterpret_cast<typename instmtype::input_char_type *>(result);
		}
		return ::fast_io::details::read_some_cold_impl(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr void read_all_impl(instmtype &insm, typename instmtype::input_char_type *first,
									typename instmtype::input_char_type *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::read_all_impl(unlocked, first, last);
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
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
		{
			auto curr{ibuffer_curr(insm)};
			auto ed{ibuffer_end(insm)};
			::std::size_t const bfddiff{
				::fast_io::details::ibuffer_remaining_size(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::input_pointer_range_size(first, last)};
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), first);
				ibuffer_set_curr(
					insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
				return;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_any_of_read_operations<instmtype> &&
			sizeof(typename instmtype::input_char_type) == 1u &&
			!::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::read_all_bytes_abi_value_direct_impl(
				insm, reinterpret_cast<::std::byte *>(first),
				reinterpret_cast<::std::byte *>(last));
		}
		// Without a get area, native All is the ordinary data plane, not a buffered underflow slow path.
		// HasAll(I) contracts the terminal graph to exactly one All(I, [first,last)) invocation, preserving
		// the named observer, empty-interval observation, and failure order. Expose only this completion leaf:
		// buffered misses and partial-progress state machines retain their existing cold-dispatch code shape.
		if constexpr (!::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype> &&
			::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
		{
			read_all_underflow_define(insm, first, last);
		}
		else
		{
			::fast_io::details::read_all_cold_impl(insm, first, last);
		}
	}
}

template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_impl(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::read_some_bytes_impl(unlocked, first, last);
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
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype> &&
					  sizeof(char_type) == 1)
		{
			auto curr{ibuffer_curr(insm)};
			auto ed{ibuffer_end(insm)};
			::std::size_t const bfddiff{
				::fast_io::details::ibuffer_remaining_size(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::input_pointer_range_size(first, last)};
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type *;
				non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), reinterpret_cast<char_type_ptr>(first));
				ibuffer_set_curr(
					insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
				return last;
			}
		}
		if constexpr (::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::read_some_bytes_abi_value_direct_impl(
				insm, first, last);
		}
		return ::fast_io::details::read_some_bytes_cold_impl(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr void read_all_bytes_impl(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::read_all_bytes_impl(unlocked, first, last);
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
		using char_type = typename instmtype::input_char_type;
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype> &&
					  sizeof(char_type) == 1)
		{
			auto curr{ibuffer_curr(insm)};
			auto ed{ibuffer_end(insm)};
			::std::size_t const bfddiff{
				::fast_io::details::ibuffer_remaining_size(curr, ed)};
			::std::size_t const itdiff{
				::fast_io::details::input_pointer_range_size(first, last)};
			if (itdiff <= bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				using char_type_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type *;
				non_overlapped_copy_n(curr, static_cast<::std::size_t>(itdiff), reinterpret_cast<char_type_ptr>(first));
				ibuffer_set_curr(
					insm, ::fast_io::details::input_pointer_advance(curr, itdiff));
				return;
			}
		}
		if constexpr (
			!::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype> &&
			!::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<instmtype> &&
			::fast_io::details::abi_value_direct_read_some_bytes<instmtype>)
		{
			return ::fast_io::details::read_all_bytes_abi_value_direct_impl(
				insm, first, last);
		}
		// Native byte-all is the first terminal alternative and an unbuffered stream's ordinary data plane.
		// Direct visibility changes no protocol decision, publication, or exception boundary. Keeping the
		// buffered branch cold avoids importing its underflow-only provider into an otherwise hot buffer loop.
		if constexpr (!::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype> &&
			::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<instmtype>)
		{
			read_all_bytes_underflow_define(insm, first, last);
		}
		else
		{
			::fast_io::details::read_all_bytes_cold_impl(insm, first, last);
		}
	}
}

} // namespace details

} // namespace fast_io

#pragma once

/*
 * Bridge from a strlike destination to an output observer.
 *
 * Concat and staging algorithms can use this non-owning wrapper when a generic
 * print/write path should target an existing string-like object. It translates
 * strlike growth/buffer CPOs into output-stream capabilities while preserving
 * the destination's ownership and writable-memory proofs. It is an internal
 * protocol adapter, not a public string and not a formatting frontend.
 */

namespace fast_io
{

/// @brief Non-owning output-stream adapter for a string-like object.
/// @details The adapter never changes ownership of `T`; it exposes either the underlying writable-buffer protocol or
///          its append/growth operations. Copies of the adapter therefore refer to the same destination and are cheap
///          stream handles, while the caller remains responsible for the lifetime of the pointed-to string-like object.
template <::std::integral ch_type, typename T>
struct io_strlike_reference_wrapper
{
	using value_type = T;
	using native_handle_type = value_type *;
	using char_type = ch_type;
	using output_char_type = char_type;
	using pointer = char_type *;
	using const_pointer = char_type const *;
	native_handle_type ptr{};
	inline constexpr native_handle_type release() noexcept
	{
		auto temp{ptr};
		ptr = nullptr;
		return temp;
	}
	inline constexpr native_handle_type native_handle() const noexcept
	{
		return ptr;
	}
};

/// @brief Preserves an exact string-like owner's writable-memory proof through the generic output adapter.
/// @details `io_strlike_reference_wrapper` adds no backing storage: its cursor operations always project the same `T`
///          object. Forwarding an existing write marker is therefore sound, while inferring cacheability from
///          `buffer_strlike` syntax would certify arbitrary user allocators and device-backed adapters. No read marker
///          is forwarded here. This wrapper is an output protocol, and a mutable put area does not by itself advertise
///          a live readable prefix to a scan or retained-scatter operation.
template <::std::integral char_type, typename T>
	requires(::fast_io::prfch_cacheable_write_provenance<T>)
inline constexpr ::std::true_type prfch_cacheable_write_provenance_define(
	io_type_t<io_strlike_reference_wrapper<char_type, T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
[[nodiscard]] inline constexpr io_strlike_reference_wrapper<char_type, T>
output_stream_ref_define(io_strlike_reference_wrapper<char_type, T> bref) noexcept
{
	return bref;
}

template <::std::integral char_type, typename T>
	requires buffered_print_preferred_strlike<char_type, T>
inline constexpr ::std::true_type
print_buffered_preferred_stream(io_reserve_type_t<char_type, io_strlike_reference_wrapper<char_type, T>>) noexcept
{
	// Forward only an underlying implementation's explicit cost promise. The wrapper contributes no allocation or
	// growth behavior of its own, so structural string-like conformance cannot manufacture this policy.
	return {};
}

/// @brief Forwards deferred-commit safety from either ordinary or runtime-only string put-area protocols.
template <::std::integral char_type, typename T>
	requires(deferred_obuffer_commit_safe_strlike<char_type, T> ||
			 runtime_deferred_obuffer_commit_safe_strlike<char_type, T>)
inline constexpr ::std::true_type print_deferred_obuffer_commit_safe(
	io_reserve_type_t<char_type, io_strlike_reference_wrapper<char_type, T>>) noexcept
{
	// Forward only the underlying implementation's strong semantic promise. In particular, T's associated namespace
	// may add status or locking hooks for this wrapper, so the fact that cursor expressions exist is not proof that raw
	// copying and one final publication preserve the complete output protocol.
	return {};
}

namespace details
{

// Keep exception probing behind `if constexpr`.  A conditional expression in a noexcept-specification would still
// require both CPO spellings to be well formed, rejecting an ordinary buffer which intentionally has no runtime CPOs
// (and vice versa).
/// @brief Computes whether the selected ordinary or runtime begin-cursor CPO is non-throwing.
template <::std::integral char_type, typename T>
inline consteval bool output_buffer_strlike_begin_nothrow() noexcept
{
	// Select only the CPO family proved by the destination's ordinary-versus-runtime buffer concept.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return noexcept(strlike_begin(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
	else
	{
		return noexcept(strlike_runtime_begin(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
}

/// @brief Computes whether the selected ordinary or runtime current-cursor CPO is non-throwing.
template <::std::integral char_type, typename T>
inline consteval bool output_buffer_strlike_curr_nothrow() noexcept
{
	// Probe the logical cursor through the same CPO family that execution will call.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return noexcept(strlike_curr(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
	else
	{
		return noexcept(strlike_runtime_curr(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
}

/// @brief Computes whether the selected ordinary or runtime end-cursor CPO is non-throwing.
template <::std::integral char_type, typename T>
inline consteval bool output_buffer_strlike_end_nothrow() noexcept
{
	// Keep the put-area endpoint exception proof aligned with the selected string adapter family.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return noexcept(strlike_end(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
	else
	{
		return noexcept(strlike_runtime_end(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>()));
	}
}

/// @brief Computes whether cursor publication through the selected string CPO is non-throwing.
template <::std::integral char_type, typename T>
inline consteval bool output_buffer_strlike_set_curr_nothrow() noexcept
{
	// Cursor publication must test the exact ordinary or runtime CPO later used by the wrapper.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return noexcept(strlike_set_curr(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>(),
			::std::declval<char_type *>()));
	}
	else
	{
		return noexcept(strlike_runtime_set_curr(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>(),
			::std::declval<char_type *>()));
	}
}

/// @brief Computes whether growth through the selected ordinary or runtime reserve CPO is non-throwing.
template <::std::integral char_type, typename T>
inline consteval bool output_buffer_strlike_reserve_nothrow() noexcept
{
	// Reserve exception behavior belongs to the active adapter family and cannot be inferred from the other family.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return noexcept(strlike_reserve(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>(),
			::std::declval<::std::size_t>()));
	}
	else
	{
		return noexcept(strlike_runtime_reserve(
			::fast_io::io_strlike_type<char_type, T>, ::std::declval<T &>(),
			::std::declval<::std::size_t>()));
	}
}

} // namespace details

/// @brief Exposes the writable-begin cursor through the string adapter family proved by its output concept.
template <::std::integral char_type, typename T>
	requires output_buffer_strlike<char_type, T>
inline constexpr char_type *obuffer_begin(io_strlike_reference_wrapper<char_type, T> bref)
	noexcept(::fast_io::details::output_buffer_strlike_begin_nothrow<char_type, T>())
{
	// Ordinary buffers expose their portable cursor CPO; runtime-only adapters use the audited implementation CPO.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return strlike_begin(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
	else
	{
		return strlike_runtime_begin(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
}

/// @brief Exposes the logical output cursor through the active ordinary or runtime string protocol.
template <::std::integral char_type, typename T>
	requires output_buffer_strlike<char_type, T>
inline constexpr char_type *obuffer_curr(io_strlike_reference_wrapper<char_type, T> bref)
	noexcept(::fast_io::details::output_buffer_strlike_curr_nothrow<char_type, T>())
{
	// Read the logical cursor from the same adapter family selected during exception probing.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return strlike_curr(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
	else
	{
		return strlike_runtime_curr(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
}

/// @brief Exposes the writable-capacity endpoint through the active string adapter protocol.
template <::std::integral char_type, typename T>
	requires output_buffer_strlike<char_type, T>
inline constexpr char_type *obuffer_end(io_strlike_reference_wrapper<char_type, T> bref)
	noexcept(::fast_io::details::output_buffer_strlike_end_nothrow<char_type, T>())
{
	// Read writable capacity through the portable protocol when present and the runtime protocol otherwise.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return strlike_end(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
	else
	{
		return strlike_runtime_end(::fast_io::io_strlike_type<char_type, T>, *bref.ptr);
	}
}

/// @brief Publishes a new logical cursor through exactly the protocol that supplied the put area.
template <::std::integral char_type, typename T>
	requires output_buffer_strlike<char_type, T>
inline constexpr void obuffer_set_curr(io_strlike_reference_wrapper<char_type, T> bref, char_type *i)
	noexcept(::fast_io::details::output_buffer_strlike_set_curr_nothrow<char_type, T>())
{
	// Publish through exactly one adapter family so the string's logical size and terminator remain synchronized.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return strlike_set_curr(::fast_io::io_strlike_type<char_type, T>, *bref.ptr, i);
	}
	else
	{
		return strlike_runtime_set_curr(::fast_io::io_strlike_type<char_type, T>, *bref.ptr, i);
	}
}

/// @brief Converts an incremental reserve request to absolute capacity and grows through the active string protocol.
template <::std::integral char_type, typename T>
	requires output_buffer_strlike<char_type, T>
inline constexpr void obuffer_flush_reserve_define(
	io_strlike_reference_wrapper<char_type, T> bref, ::std::size_t to_reserve)
	noexcept(
		::fast_io::details::output_buffer_strlike_begin_nothrow<char_type, T>() &&
		::fast_io::details::output_buffer_strlike_curr_nothrow<char_type, T>() &&
		::fast_io::details::output_buffer_strlike_reserve_nothrow<char_type, T>())
{
	// String growth is an allocation boundary and may throw. Mirroring all queried CPOs keeps a throwing allocator or
	// user adapter observable to the caller instead of converting an ordinary reserve failure into termination.
	auto &strref{*bref.ptr};
	to_reserve = ::fast_io::details::intrinsics::add_or_overflow_die(
		static_cast<::std::size_t>(obuffer_curr(bref) - obuffer_begin(bref)), to_reserve);
	// Preserve the destination's active growth CPO after converting the request to an absolute capacity.
	if constexpr (buffer_strlike<char_type, T>)
	{
		return strlike_reserve(::fast_io::io_strlike_type<char_type, T>, strref, to_reserve);
	}
	else
	{
		return strlike_runtime_reserve(::fast_io::io_strlike_type<char_type, T>, strref, to_reserve);
	}
}

namespace details
{

template <::std::size_t size_char_type>
inline constexpr ::std::size_t cal_new_cap_io_strlike(::std::size_t cap) noexcept
{
	::std::size_t new_cap{};
	if (cap == 0)
	{
		new_cap = 1;
	}
	else
	{
		constexpr ::std::size_t mx_size{SIZE_MAX / size_char_type};
		constexpr ::std::size_t mx_div2{static_cast<::std::size_t>(mx_size / 2u)};
		if (mx_size == cap) [[unlikely]]
		{
			fast_terminate();
		}
		else if (cap >= mx_div2)
		{
			new_cap = mx_size;
		}
		else
		{
			new_cap = cap;
			new_cap <<= 1u;
		}
	}
	return new_cap;
}

} // namespace details

/// @brief Refills a string-backed output area while preserving its ordinary-versus-runtime growth protocol.
template <::std::integral ch_type, typename T>
	requires output_buffer_strlike<ch_type, T>
inline constexpr void output_stream_buffer_flush_define(io_strlike_reference_wrapper<ch_type, T> bref)
	noexcept(
		::fast_io::details::output_buffer_strlike_begin_nothrow<ch_type, T>() &&
		::fast_io::details::output_buffer_strlike_end_nothrow<ch_type, T>() &&
		::fast_io::details::output_buffer_strlike_reserve_nothrow<ch_type, T>())
{
	// The refill protocol has the same exception boundary as explicit reserve above; cursor inspection and growth are
	// all represented in the conditional specification so concept dispatch never invents a stronger guarantee.
	auto &strref{*bref.ptr};
	auto bptr{obuffer_begin(bref)};
	auto eptr{obuffer_end(bref)};
	auto cap{static_cast<::std::size_t>(eptr - bptr)};
	auto const new_capacity{
		::fast_io::details::cal_new_cap_io_strlike<sizeof(ch_type)>(cap)};
	// Refill must grow through the same adapter family that supplied the cursor pair above.
	if constexpr (buffer_strlike<ch_type, T>)
	{
		strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_capacity);
	}
	else
	{
		strlike_runtime_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_capacity);
	}
}

/// @brief Handles one-character overflow through public append support or audited private-capacity growth.
template <::std::integral ch_type, typename T>
	requires output_buffer_strlike<ch_type, T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void obuffer_overflow(io_strlike_reference_wrapper<ch_type, T> bref, ch_type ch)
{
	auto &strref{*bref.ptr};
	// Auxiliary strings own a public push-back protocol and do not require private put-area growth.
	if constexpr (auxiliary_strlike<ch_type, T>)
	{
		strlike_push_back(::fast_io::io_strlike_type<ch_type, T>, strref, ch);
	}
	else
	{
		auto bptr{obuffer_begin(bref)};
		auto eptr{obuffer_end(bref)};
		auto cap{static_cast<::std::size_t>(eptr - bptr)};
		auto const new_capacity{
			::fast_io::details::cal_new_cap_io_strlike<sizeof(ch_type)>(cap)};
		// Non-auxiliary buffers retain their ordinary-versus-runtime reserve distinction on overflow.
		if constexpr (buffer_strlike<ch_type, T>)
		{
			strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_capacity);
		}
		else
		{
			strlike_runtime_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_capacity);
		}
		auto curr_ptr{obuffer_curr(bref)};
		*curr_ptr = ch;
		++curr_ptr;
		obuffer_set_curr(bref, curr_ptr);
	}
}

/// @brief Completes a range overflow while reacquiring cursors after any string-storage relocation.
template <::std::integral ch_type, typename T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void write_all_overflow_define(io_strlike_reference_wrapper<ch_type, T> bref, ch_type const *first,
												ch_type const *last)
{
	if (first == last)
	{
		// The overflow CPO remains a valid direct customization point. An empty range must not inspect lazy null cursors
		// or turn a no-op into the destination's first allocation when a caller reaches the CPO without core dispatch.
		return;
	}
	auto &strref{*bref.ptr};
	// Auxiliary adapters provide an overlap-aware append operation and bypass private cursor management.
	if constexpr (auxiliary_strlike<ch_type, T>)
	{
		strlike_append(::fast_io::io_strlike_type<ch_type, T>, strref, first, last);
	}
	else
	{
		auto curr{obuffer_curr(bref)};
		::std::size_t const bufferdiff{
			static_cast<::std::size_t>(obuffer_end(bref) - curr)};
		curr = ::fast_io::freestanding::non_overlapped_copy_n(first, bufferdiff, curr);
		first += bufferdiff;
		obuffer_set_curr(bref, curr);
		if (first == last)
		{
			// A defensive exact-fit boundary is required even though core now handles equality in its hot path. Another
			// conforming caller may enter this overflow CPO at the exact-capacity boundary; the copied prefix already
			// completes that request.
			return;
		}
		auto bptr{obuffer_begin(bref)};
		auto eptr{obuffer_end(bref)};
		auto cap{static_cast<::std::size_t>(eptr - bptr)};
		::std::size_t new_cap{::fast_io::details::cal_new_cap_io_strlike<sizeof(ch_type)>(cap)};
		::std::size_t const size_minimum{
			::fast_io::details::intrinsics::add_or_overflow_die(static_cast<::std::size_t>(last - first), cap)};
		if (new_cap < size_minimum)
		{
			new_cap = size_minimum;
		}
		// Grow through the protocol that owns the cursor representation before reacquiring relocated pointers.
		if constexpr (buffer_strlike<ch_type, T>)
		{
			strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_cap);
		}
		else
		{
			strlike_runtime_reserve(::fast_io::io_strlike_type<ch_type, T>, strref, new_cap);
		}
		auto curr_ptr{obuffer_curr(bref)};
		curr_ptr = ::fast_io::freestanding::non_overlapped_copy(first, last, curr_ptr);
		obuffer_set_curr(bref, curr_ptr);
	}
}

} // namespace fast_io

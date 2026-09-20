#pragma once

/*
 * Public seek and flush operations (primitive IO level).
 *
 * These algorithms normalize a stream once, traverse complete mutex/unlocked
 * projections, flush the appropriate buffer when required, and dispatch to
 * element- or byte-based seek CPOs with checked conversions. The capability
 * shapes are declared in `refs/seek.h`; this file gives them operation
 * semantics and synchronization.
 */

namespace fast_io::operations
{

namespace decay
{

/*
 * Seek primitives expose the same two transport contracts as read and write.
 * The historical unsuffixed `*_decay` entry is a value owner and therefore
 * retains the platform aggregate ABI visible to address-taking callers.  The
 * `_borrowed` entry is the sole recursive graph: it preserves mutable observer
 * identity across buffer publication and every mutex/unlocked edge.  A named
 * normalized observer reaches the value owner only through the mandatory-
 * inline `_dispatch` bridge and only when the stream author has supplied the
 * `stream_ref_value_transport_safe_define` substitution proof and the target
 * ABI policy admits that representation.  Thus size or triviality alone never
 * turns an inline cursor into a discarded copy, while proven descriptor-like
 * proxies may still arrive in argument registers.
 */

namespace defines
{

template <typename T>
concept has_any_stream_mutex_ref_define =
	::fast_io::operations::decay::defines::has_input_stream_mutex_ref_define<::std::remove_cvref_t<T>> ||
	::fast_io::operations::decay::defines::has_output_stream_mutex_ref_define<::std::remove_cvref_t<T>> ||
	::fast_io::operations::decay::defines::has_io_stream_mutex_ref_define<::std::remove_cvref_t<T>>;

enum class seek_dispatch_operation
{
	buffer_flush,
	seek_bytes,
	seek
};

namespace seek_dispatch_details
{

/// @brief Tests the terminal input capability selected after every mutex layer has been removed.
/// @details A complete mutex protocol proves storage, character-domain preservation, and finite type progress, but it
///          does not prove that the final unlocked observer implements this particular operation. Recursing in the
///          constraint mirrors the executable dispatch exactly. Consequently a well-formed lock wrapper around a
///          non-seekable terminal is rejected during substitution instead of failing in the selected function body.
///          The public helper proves the whole chain once; this `_after_complete_chain` walk deliberately does not
///          re-prove every suffix. Thus an N-layer wrapper costs O(N), rather than the O(N^2) instantiations caused by
///          nesting a full finite-chain proof at each edge.
template <seek_dispatch_operation operation, typename T>
inline consteval bool input_stream_operation_after_complete_chain_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<observer_type>)
	{
		using unlocked_type = ::std::remove_cvref_t<decltype(::fast_io::operations::decay::input_stream_unlocked_ref_decay(::std::declval<observer_type &>()))>;
		return input_stream_operation_after_complete_chain_impl<operation, unlocked_type>();
	}
	else if constexpr (operation == seek_dispatch_operation::buffer_flush)
	{
		return ::fast_io::operations::decay::defines::has_input_or_io_stream_buffer_flush_define<observer_type>;
	}
	else if constexpr (operation == seek_dispatch_operation::seek_bytes)
	{
		return ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<observer_type>;
	}
	else
	{
		return ::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<observer_type>;
	}
}

template <seek_dispatch_operation operation, typename T>
inline consteval bool input_stream_operation_dispatchable_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<observer_type> &&
				  !::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<observer_type>)
	{
		return false;
	}
	else
	{
		return input_stream_operation_after_complete_chain_impl<operation, observer_type>();
	}
}

/// @brief Output-direction counterpart of `input_stream_operation_dispatchable_impl`.
template <seek_dispatch_operation operation, typename T>
inline consteval bool output_stream_operation_after_complete_chain_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<observer_type>)
	{
		using unlocked_type = ::std::remove_cvref_t<decltype(::fast_io::operations::decay::output_stream_unlocked_ref_decay(::std::declval<observer_type &>()))>;
		return output_stream_operation_after_complete_chain_impl<operation, unlocked_type>();
	}
	else if constexpr (operation == seek_dispatch_operation::buffer_flush)
	{
		return ::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<observer_type>;
	}
	else if constexpr (operation == seek_dispatch_operation::seek_bytes)
	{
		return ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<observer_type>;
	}
	else
	{
		return ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<observer_type>;
	}
}

template <seek_dispatch_operation operation, typename T>
inline consteval bool output_stream_operation_dispatchable_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<observer_type> &&
				  !::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<observer_type>)
	{
		return false;
	}
	else
	{
		return output_stream_operation_after_complete_chain_impl<operation, observer_type>();
	}
}

/// @brief Joint-direction counterpart for io seek and flush operations.
/// @details Any directional marker blocks the direct io CPO because bypassing it would violate its synchronization
///          policy. Only a complete io-specific protocol proves that one guard protects both directions; after that
///          proof, recursion follows the exact io-unlocked CPO until the terminal capability is reached.
template <seek_dispatch_operation operation, typename T>
inline consteval bool io_stream_operation_after_complete_chain_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_io_stream_mutex_ref_define<observer_type>)
	{
		using unlocked_type = ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(::std::declval<observer_type &>()))>;
		return io_stream_operation_after_complete_chain_impl<operation, unlocked_type>();
	}
	else if constexpr (::fast_io::operations::decay::defines::has_any_stream_mutex_ref_define<observer_type>)
	{
		// A directional marker cannot be consumed by joint io dispatch. It still owns synchronization policy, so the
		// direct io operation must not bypass it even after an outer io layer has been removed.
		return false;
	}
	else if constexpr (operation == seek_dispatch_operation::buffer_flush)
	{
		return ::fast_io::operations::decay::defines::has_io_stream_buffer_flush_define<observer_type>;
	}
	else if constexpr (operation == seek_dispatch_operation::seek_bytes)
	{
		return ::fast_io::operations::decay::defines::has_io_stream_seek_bytes_define<observer_type>;
	}
	else
	{
		return ::fast_io::operations::decay::defines::has_io_stream_seek_define<observer_type>;
	}
}

template <seek_dispatch_operation operation, typename T>
inline consteval bool io_stream_operation_dispatchable_impl() noexcept
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_any_stream_mutex_ref_define<observer_type> &&
				  !::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<observer_type>)
	{
		return false;
	}
	else
	{
		return io_stream_operation_after_complete_chain_impl<operation, observer_type>();
	}
}

} // namespace seek_dispatch_details

/// A mutex marker owns dispatch policy for its direction. A direct CPO must not bypass a malformed lock wrapper.
/// Operation admission therefore follows the finite unlocked chain and verifies the selected terminal CPO.
template <typename T>
concept input_stream_buffer_flush_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::input_stream_operation_dispatchable_impl<
		seek_dispatch_operation::buffer_flush, T>();

template <typename T>
concept output_stream_buffer_flush_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::output_stream_operation_dispatchable_impl<
		seek_dispatch_operation::buffer_flush, T>();

template <typename T>
concept io_stream_buffer_flush_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::io_stream_operation_dispatchable_impl<
		seek_dispatch_operation::buffer_flush, T>();

template <typename T>
concept input_stream_seek_bytes_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::input_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek_bytes, T>();

template <typename T>
concept output_stream_seek_bytes_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::output_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek_bytes, T>();

template <typename T>
concept io_stream_seek_bytes_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::io_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek_bytes, T>();

template <typename T>
concept input_stream_seek_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::input_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek, T>();

template <typename T>
concept output_stream_seek_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::output_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek, T>();

template <typename T>
concept io_stream_seek_dispatchable =
	::fast_io::operations::decay::defines::seek_dispatch_details::io_stream_operation_dispatchable_impl<
		seek_dispatch_operation::seek, T>();

} // namespace defines

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_buffer_flush_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_buffer_flush_decay_borrowed(T &t)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<observer_type>)
	{
		// The complete protocol proves that the guard is storable, preserves the input character domain, and unwraps
		// to a different type. Holding the guard across recursive dispatch makes buffer-state publication atomic.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(t)};
		// `decltype(auto)` owns a prvalue unlocked proxy but preserves a stable reference result. Passing the named
		// local then borrows either representation; recursive synchronization never copies an observer.
		decltype(auto) unlocked{::fast_io::operations::decay::input_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed(unlocked);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_stream_buffer_flush_define<observer_type>)
	{
		return input_stream_buffer_flush_define(t);
	}
	else
	{
		return io_stream_buffer_flush_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_buffer_flush_dispatchable<T>)
inline constexpr void input_stream_buffer_flush_decay(T t)
{
	return ::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed(t);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_buffer_flush_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void input_stream_buffer_flush_decay_dispatch(T &t)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::input_stream_buffer_flush_decay(t);
	}
	else
	{
		return ::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_buffer_flush_decay_borrowed(T &t)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::output_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed(unlocked);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_stream_buffer_flush_define<observer_type>)
	{
		return output_stream_buffer_flush_define(t);
	}
	else
	{
		return io_stream_buffer_flush_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<T>)
inline constexpr void output_stream_buffer_flush_decay(T t)
{
	return ::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed(t);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void output_stream_buffer_flush_decay_dispatch(T &t)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::output_stream_buffer_flush_decay(t);
	}
	else
	{
		return ::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_buffer_flush_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_buffer_flush_decay_borrowed(T &t)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<observer_type>)
	{
		// An io flush consumes both directions' pending state, so only the io-specific mutex/unlocked pair can prove
		// that one critical section protects the whole state transition.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::io_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::io_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::io_stream_buffer_flush_decay_borrowed(unlocked);
	}
	else
	{
		return io_stream_buffer_flush_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_buffer_flush_dispatchable<T>)
inline constexpr void io_stream_buffer_flush_decay(T t)
{
	return ::fast_io::operations::decay::io_stream_buffer_flush_decay_borrowed(t);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_buffer_flush_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void io_stream_buffer_flush_decay_dispatch(T &t)
{
	if constexpr (::fast_io::operations::defines::abi_value_io_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::io_stream_buffer_flush_decay(t);
	}
	else
	{
		return ::fast_io::operations::decay::io_stream_buffer_flush_decay_borrowed(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t input_stream_seek_bytes_decay_borrowed(T &t, ::fast_io::intfpos_t off,
																		 ::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::input_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::input_stream_seek_bytes_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<observer_type>)
		{
			if (skd == ::fast_io::seekdir::cur)
			{
				off = ::fast_io::details::adjust_instm_offset(ibuffer_end(t) - ibuffer_curr(t), off);
			}
		}
		if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed(t);
		}
		if constexpr (::fast_io::operations::decay::defines::has_input_stream_seek_bytes_define<observer_type>)
		{
			return input_stream_seek_bytes_define(t, off, skd);
		}
		else
		{
			return io_stream_seek_bytes_define(t, off, skd);
		}
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_bytes_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t input_stream_seek_bytes_decay(T t, ::fast_io::intfpos_t off,
																   ::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::input_stream_seek_bytes_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_bytes_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
input_stream_seek_bytes_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::input_stream_seek_bytes_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::input_stream_seek_bytes_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_rewind_bytes_decay(T t)
{
	::fast_io::operations::decay::input_stream_seek_bytes_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t output_stream_seek_bytes_decay_borrowed(T &t, ::fast_io::intfpos_t off,
																		  ::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::output_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::output_stream_seek_bytes_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed(t);
		}
		if constexpr (::fast_io::operations::decay::defines::has_output_stream_seek_bytes_define<observer_type>)
		{
			return output_stream_seek_bytes_define(t, off, skd);
		}
		else
		{
			return io_stream_seek_bytes_define(t, off, skd);
		}
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_bytes_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t output_stream_seek_bytes_decay(T t, ::fast_io::intfpos_t off,
																	::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::output_stream_seek_bytes_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_bytes_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
output_stream_seek_bytes_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::output_stream_seek_bytes_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::output_stream_seek_bytes_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_rewind_bytes_decay(T t)
{
	::fast_io::operations::decay::output_stream_seek_bytes_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t io_stream_seek_bytes_decay_borrowed(T &t, ::fast_io::intfpos_t off,
																	 ::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::io_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::io_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<observer_type>)
		{
			if (skd == ::fast_io::seekdir::cur)
			{
				off = ::fast_io::details::adjust_instm_offset(ibuffer_end(t) - ibuffer_curr(t), off);
			}
		}
		if constexpr (::fast_io::operations::decay::defines::has_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::io_stream_buffer_flush_decay_borrowed(t);
		}
		return io_stream_seek_bytes_define(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_bytes_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t io_stream_seek_bytes_decay(T t, ::fast_io::intfpos_t off,
																::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_bytes_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
io_stream_seek_bytes_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_io_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::io_stream_seek_bytes_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_bytes_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_rewind_bytes_decay(T t)
{
	::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t input_stream_seek_decay_borrowed(T &t, ::fast_io::intfpos_t off,
																   ::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::input_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::input_stream_seek_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<observer_type>)
		{
			if (skd == ::fast_io::seekdir::cur)
			{
				off = ::fast_io::details::adjust_instm_offset(ibuffer_end(t) - ibuffer_curr(t), off);
			}
		}
		if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed(t);
		}
		if constexpr (::fast_io::operations::decay::defines::has_input_stream_seek_define<observer_type>)
		{
			return input_stream_seek_define(t, off, skd);
		}
		else
		{
			return io_stream_seek_define(t, off, skd);
		}
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t input_stream_seek_decay(T t, ::fast_io::intfpos_t off,
														  ::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::input_stream_seek_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
input_stream_seek_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::input_stream_seek_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::input_stream_seek_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::input_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_rewind_decay(T t)
{
	::fast_io::operations::decay::input_stream_seek_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t output_stream_seek_decay_borrowed(T &t, ::fast_io::intfpos_t off,
																	::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::output_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::output_stream_seek_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed(t);
		}
		if constexpr (::fast_io::operations::decay::defines::has_output_stream_seek_define<observer_type>)
		{
			return output_stream_seek_define(t, off, skd);
		}
		else
		{
			return io_stream_seek_define(t, off, skd);
		}
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t output_stream_seek_decay(T t, ::fast_io::intfpos_t off,
														   ::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::output_stream_seek_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
output_stream_seek_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::output_stream_seek_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::output_stream_seek_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::output_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_rewind_decay(T t)
{
	::fast_io::operations::decay::output_stream_seek_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t io_stream_seek_decay_borrowed(T &t, ::fast_io::intfpos_t off,
														::fast_io::seekdir skd)
{
	using observer_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::io_stream_mutex_ref_decay(t)};
		decltype(auto) unlocked{::fast_io::operations::decay::io_stream_unlocked_ref_decay(t)};
		return ::fast_io::operations::decay::io_stream_seek_decay_borrowed(unlocked, off, skd);
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<observer_type>)
		{
			if (skd == ::fast_io::seekdir::cur)
			{
				off = ::fast_io::details::adjust_instm_offset(ibuffer_end(t) - ibuffer_curr(t), off);
			}
		}
		if constexpr (::fast_io::operations::decay::defines::has_io_stream_buffer_flush_define<observer_type>)
		{
			::fast_io::operations::decay::io_stream_buffer_flush_decay_borrowed(t);
		}
		return io_stream_seek_define(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_dispatchable<T>)
inline constexpr ::fast_io::intfpos_t io_stream_seek_decay(T t, ::fast_io::intfpos_t off,
													   ::fast_io::seekdir skd)
{
	return ::fast_io::operations::decay::io_stream_seek_decay_borrowed(t, off, skd);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_dispatchable<T>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::intfpos_t
io_stream_seek_decay_dispatch(T &t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	if constexpr (::fast_io::operations::defines::abi_value_io_stream_ref_result<T &>)
	{
		return ::fast_io::operations::decay::io_stream_seek_decay(t, off, skd);
	}
	else
	{
		return ::fast_io::operations::decay::io_stream_seek_decay_borrowed(t, off, skd);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::io_stream_seek_dispatchable<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_rewind_decay(T t)
{
	::fast_io::operations::decay::io_stream_seek_decay_borrowed(t, 0, ::fast_io::seekdir::beg);
}

} // namespace decay

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t input_stream_seek_bytes(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	// The normalized expression is the only owner when the stream-ref CPO returns a prvalue. `decltype(auto)` also
	// preserves the reference selected by ABI-aware normalization for a large or noncopyable lvalue observer.
	decltype(auto) observer{::fast_io::operations::input_stream_ref(t)};
	return ::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t output_stream_seek_bytes(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	decltype(auto) observer{::fast_io::operations::output_stream_ref(t)};
	return ::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t io_stream_seek_bytes(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	decltype(auto) observer{::fast_io::operations::io_stream_ref(t)};
	return ::fast_io::operations::decay::io_stream_seek_bytes_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t input_stream_seek(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	decltype(auto) observer{::fast_io::operations::input_stream_ref(t)};
	return ::fast_io::operations::decay::input_stream_seek_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t output_stream_seek(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	decltype(auto) observer{::fast_io::operations::output_stream_ref(t)};
	return ::fast_io::operations::decay::output_stream_seek_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::intfpos_t io_stream_seek(T &&t, ::fast_io::intfpos_t off, ::fast_io::seekdir skd)
{
	decltype(auto) observer{::fast_io::operations::io_stream_ref(t)};
	return ::fast_io::operations::decay::io_stream_seek_decay_dispatch(observer, off, skd);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_rewind_bytes(T &&t)
{
	decltype(auto) observer{::fast_io::operations::input_stream_ref(t)};
	::fast_io::operations::decay::input_stream_seek_bytes_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_rewind_bytes(T &&t)
{
	decltype(auto) observer{::fast_io::operations::output_stream_ref(t)};
	::fast_io::operations::decay::output_stream_seek_bytes_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_rewind_bytes(T &&t)
{
	decltype(auto) observer{::fast_io::operations::io_stream_ref(t)};
	::fast_io::operations::decay::io_stream_seek_bytes_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_rewind(T &&t)
{
	decltype(auto) observer{::fast_io::operations::input_stream_ref(t)};
	::fast_io::operations::decay::input_stream_seek_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_rewind(T &&t)
{
	decltype(auto) observer{::fast_io::operations::output_stream_ref(t)};
	::fast_io::operations::decay::output_stream_seek_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_rewind(T &&t)
{
	decltype(auto) observer{::fast_io::operations::io_stream_ref(t)};
	::fast_io::operations::decay::io_stream_seek_decay_dispatch(observer, 0, ::fast_io::seekdir::beg);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void input_stream_buffer_flush(T &&t)
{
	decltype(auto) observer{::fast_io::operations::input_stream_ref(t)};
	::fast_io::operations::decay::input_stream_buffer_flush_decay_dispatch(observer);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void output_stream_buffer_flush(T &&t)
{
	decltype(auto) observer{::fast_io::operations::output_stream_ref(t)};
	::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(observer);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void io_stream_buffer_flush(T &&t)
{
	decltype(auto) observer{::fast_io::operations::io_stream_ref(t)};
	::fast_io::operations::decay::io_stream_buffer_flush_decay_dispatch(observer);
}

} // namespace fast_io::operations

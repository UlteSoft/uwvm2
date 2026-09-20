#pragma once

/*
 * Decorator filter construction and dispatch (IO/protocol bridge).
 *
 * These operations attach directional transformation objects to streams,
 * selecting provider-defined filter construction when available and preserving
 * source category so an rvalue decorator may become owned state. Stream and
 * decorator normalization occur once; actual transformed transfer later uses
 * the ordinary read/write operation matrix.
 */

namespace fast_io
{
namespace decorators
{

struct no_op_decorators
{
};

} // namespace decorators

namespace operations::decay
{

namespace defines
{

/// @brief Recognizes the exact output-decorator expression selected by forwarding dispatch.
/// @details `P` retains its source category.  This is important for owning sinks: an rvalue decorator can be moved into
///          persistent filter storage, while an lvalue remains a caller-owned object and can be copied deliberately by
///          that sink.  The result remains unconstrained for compatibility with the historical customization vocabulary;
///          dispatch ignores it and this ownership repair must not silently turn an established CPO into a protocol break.
template <typename T, typename P>
concept has_output_stream_add_deco_filter_define = requires(T &t, P &&p) {
	output_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
};

/// @brief Input-direction counterpart of `has_output_stream_add_deco_filter_define`.
template <typename T, typename P>
concept has_input_stream_add_deco_filter_define = requires(T &t, P &&p) {
	input_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
};

/// @brief Joint-direction counterpart of `has_output_stream_add_deco_filter_define`.
template <typename T, typename P>
concept has_io_stream_add_deco_filter_define = requires(T &t, P &&p) {
	io_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
};

/// @brief Detects the legacy named-owner fallback for an rvalue decorator.
/// @details The historical public API decayed a decorator into a named local before invoking its CPO, so existing
///          customizations may intentionally accept only `Decorator&`.  The new owner path first prefers the exact
///          rvalue expression, which lets a persistent sink consume a move-only decorator without another copy.  Only
///          when that expression is unavailable may dispatch expose the same local as an lvalue.  Such a fallback is a
///          synchronous borrowing contract: the customization must not retain the address after it returns.
template <typename T, typename P>
concept has_output_stream_add_deco_filter_lvalue_fallback =
	!::std::is_lvalue_reference_v<P &&> &&
	has_output_stream_add_deco_filter_define<T, ::std::remove_reference_t<P> &>;

template <typename T, typename P>
concept has_input_stream_add_deco_filter_lvalue_fallback =
	!::std::is_lvalue_reference_v<P &&> &&
	has_input_stream_add_deco_filter_define<T, ::std::remove_reference_t<P> &>;

template <typename T, typename P>
concept has_io_stream_add_deco_filter_lvalue_fallback =
	!::std::is_lvalue_reference_v<P &&> &&
	has_io_stream_add_deco_filter_define<T, ::std::remove_reference_t<P> &>;

template <typename T, typename P>
concept output_stream_add_deco_filter_argument =
	::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators> ||
	has_output_stream_add_deco_filter_define<T, P> ||
	has_output_stream_add_deco_filter_lvalue_fallback<T, P>;

template <typename T, typename P>
concept input_stream_add_deco_filter_argument =
	::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators> ||
	has_input_stream_add_deco_filter_define<T, P> ||
	has_input_stream_add_deco_filter_lvalue_fallback<T, P>;

template <typename T, typename P>
concept io_stream_add_deco_filter_argument =
	::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators> ||
	has_io_stream_add_deco_filter_define<T, P> ||
	has_io_stream_add_deco_filter_lvalue_fallback<T, P>;

template <typename T, typename P>
concept has_output_or_io_stream_add_deco_filter_define =
	output_stream_add_deco_filter_argument<T, P> || io_stream_add_deco_filter_argument<T, P>;

template <typename T, typename P>
concept has_input_or_io_stream_add_deco_filter_define =
	input_stream_add_deco_filter_argument<T, P> || io_stream_add_deco_filter_argument<T, P>;

} // namespace defines

/// @brief Adds one output decorator without reopening a by-value boundary in the internal call graph.
/// @details Directional CPO precedence is preserved independently of value category.  Within the selected direction the
///          exact forwarded expression wins; the named-lvalue compatibility branch is used only when no rvalue-capable
///          expression exists.  Rvalue callability alone is not an ownership proof (`const P&` is also callable), so a
///          persistent CPO must still decay its own storage.  `p` is never copied merely to cross this helper.
template <typename T, typename P>
		requires(::fast_io::operations::decay::defines::has_output_or_io_stream_add_deco_filter_define<T, P>)
inline constexpr void output_stream_add_deco_filter_decay(T &t, P &&p)
{
	if constexpr (!::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators>)
	{
		if constexpr (::fast_io::operations::decay::defines::output_stream_add_deco_filter_argument<T, P>)
		{
			if constexpr (::fast_io::operations::decay::defines::has_output_stream_add_deco_filter_define<T, P>)
			{
				output_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
			}
			else
			{
				output_stream_add_deco_filter_define(t, p);
			}
		}
		else
		{
			if constexpr (::fast_io::operations::decay::defines::has_io_stream_add_deco_filter_define<T, P>)
			{
				io_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
			}
			else
			{
				io_stream_add_deco_filter_define(t, p);
			}
		}
	}
}

template <typename T, typename P>
		requires(::fast_io::operations::decay::defines::has_input_or_io_stream_add_deco_filter_define<T, P>)
inline constexpr void input_stream_add_deco_filter_decay(T &t, P &&p)
{
	if constexpr (!::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators>)
	{
		if constexpr (::fast_io::operations::decay::defines::input_stream_add_deco_filter_argument<T, P>)
		{
			if constexpr (::fast_io::operations::decay::defines::has_input_stream_add_deco_filter_define<T, P>)
			{
				input_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
			}
			else
			{
				input_stream_add_deco_filter_define(t, p);
			}
		}
		else
		{
			if constexpr (::fast_io::operations::decay::defines::has_io_stream_add_deco_filter_define<T, P>)
			{
				io_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
			}
			else
			{
				io_stream_add_deco_filter_define(t, p);
			}
		}
	}
}

template <typename T, typename P>
		requires(::fast_io::operations::decay::defines::io_stream_add_deco_filter_argument<T, P>)
inline constexpr void io_stream_add_deco_filter_decay(T &t, P &&p)
{
	if constexpr (!::std::same_as<::std::remove_cvref_t<P>, ::fast_io::decorators::no_op_decorators>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_io_stream_add_deco_filter_define<T, P>)
		{
			io_stream_add_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
		}
		else
		{
			io_stream_add_deco_filter_define(t, p);
		}
	}
}

/// @brief Applies a decorator pack while borrowing the already-normalized stream reference.
/// @details Each decorator retains its incoming category through exactly one per-element helper.  No pack suffix and no
///          internal fallback owns another copy; a public by-value entry can therefore move its sole local owner into a
///          persistent CPO, and transcode composition can prove the same expression it executes.
template <typename T, typename... Args>
		requires(::fast_io::operations::decay::defines::has_output_or_io_stream_add_deco_filter_define<T, Args> && ...)
inline constexpr void add_output_decos_decay(T &t, Args &&...args)
{
	(output_stream_add_deco_filter_decay(t, ::fast_io::freestanding::forward<Args>(args)), ...);
}

template <typename T, typename... Args>
		requires(::fast_io::operations::decay::defines::has_input_or_io_stream_add_deco_filter_define<T, Args> && ...)
inline constexpr void add_input_decos_decay(T &t, Args &&...args)
{
	(input_stream_add_deco_filter_decay(t, ::fast_io::freestanding::forward<Args>(args)), ...);
}

template <typename T, typename... Args>
		requires(::fast_io::operations::decay::defines::io_stream_add_deco_filter_argument<T, Args> && ...)
inline constexpr void add_io_decos_decay(T &t, Args &&...args)
{
	(io_stream_add_deco_filter_decay(t, ::fast_io::freestanding::forward<Args>(args)), ...);
}

} // namespace operations::decay

namespace operations
{

namespace defines
{

template <typename result_type>
concept storable_stream_deco_filter_ref_result =
	::fast_io::operations::defines::storable_stream_ref_result_object<result_type>();

/// @brief Selects the only decorator-reference result category that may remain borrowed.
/// @details A mutable lvalue has storage supplied by the CPO author and is consumed synchronously by add/transcode
///          dispatch.  Every prvalue, xvalue, and cv-qualified lvalue is materialized once by the public reference helper;
///          otherwise a returned xvalue could escape as a dangling reference and a const proxy could not model mutable
///          decorator state.  Unlike primitive stream transport, this boundary deliberately performs no ABI-small lvalue
///          copy: decorator identity may contain incremental codec state and no independent copy-equivalence marker exists.
template <typename result_type>
inline constexpr bool stream_deco_filter_ref_result_borrows_lvalue =
	::std::is_lvalue_reference_v<result_type> &&
	!::std::is_const_v<::std::remove_reference_t<result_type>> &&
	!::std::is_volatile_v<::std::remove_reference_t<result_type>>;

template <typename T>
concept has_input_stream_deco_filter_ref_define = requires(T &&t) {
	input_stream_deco_filter_ref_define(t);
	requires ::fast_io::operations::defines::storable_stream_deco_filter_ref_result<
		decltype(input_stream_deco_filter_ref_define(t))>;
};

template <typename T>
concept has_output_stream_deco_filter_ref_define = requires(T &&t) {
	output_stream_deco_filter_ref_define(t);
	requires ::fast_io::operations::defines::storable_stream_deco_filter_ref_result<
		decltype(output_stream_deco_filter_ref_define(t))>;
};

template <typename T>
concept has_io_stream_deco_filter_ref_define = requires(T &&t) {
	io_stream_deco_filter_ref_define(t);
	requires ::fast_io::operations::defines::storable_stream_deco_filter_ref_result<
		decltype(io_stream_deco_filter_ref_define(t))>;
};

template <typename T>
concept has_input_or_io_stream_deco_filter_ref_define =
	has_input_stream_deco_filter_ref_define<T> || has_io_stream_deco_filter_ref_define<T>;

template <typename T>
concept has_output_or_io_stream_deco_filter_ref_define =
	has_output_stream_deco_filter_ref_define<T> || has_io_stream_deco_filter_ref_define<T>;

} // namespace defines

template <typename T>
	requires(::fast_io::operations::defines::has_input_or_io_stream_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) input_stream_deco_filter_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_input_stream_deco_filter_ref_define<T>)
	{
		using result_type = decltype(input_stream_deco_filter_ref_define(t));
		if constexpr (::fast_io::operations::defines::stream_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return input_stream_deco_filter_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(input_stream_deco_filter_ref_define(t));
		}
	}
	else
	{
		using result_type = decltype(io_stream_deco_filter_ref_define(t));
		if constexpr (::fast_io::operations::defines::stream_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_deco_filter_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_stream_deco_filter_ref_define(t));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_output_or_io_stream_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) output_stream_deco_filter_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_output_stream_deco_filter_ref_define<T>)
	{
		using result_type = decltype(output_stream_deco_filter_ref_define(t));
		if constexpr (::fast_io::operations::defines::stream_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return output_stream_deco_filter_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(output_stream_deco_filter_ref_define(t));
		}
	}
	else
	{
		using result_type = decltype(io_stream_deco_filter_ref_define(t));
		if constexpr (::fast_io::operations::defines::stream_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_deco_filter_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_stream_deco_filter_ref_define(t));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_io_stream_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) io_stream_deco_filter_ref(T &&t)
{
	using result_type = decltype(io_stream_deco_filter_ref_define(t));
	if constexpr (::fast_io::operations::defines::stream_deco_filter_ref_result_borrows_lvalue<result_type>)
	{
		return io_stream_deco_filter_ref_define(t);
	}
	else
	{
		return ::std::remove_cvref_t<result_type>(io_stream_deco_filter_ref_define(t));
	}
}

namespace defines
{

/// @brief Proves the complete public output-decorator operation against its executed expression categories.
/// @details The stream object is named before reference normalization.  Each decorator is likewise a by-value public
///          owner, but is offered to the decay graph as an rvalue exactly once.  The requires-expression therefore sees
///          the same normalized reference lvalue and the same exact-rvalue-or-legacy-fallback choice as the function body;
///          a malformed pack becomes constraint-false instead of failing after overload selection.
template <typename T, typename... Args>
inline consteval bool has_add_output_decos() noexcept
{
	using source_type = ::std::remove_reference_t<T>;
	if constexpr (!::fast_io::operations::defines::has_output_or_io_stream_deco_filter_ref_define<source_type &>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::output_stream_deco_filter_ref(
			::std::declval<source_type &>()));
		using named_ref_type = ::std::remove_reference_t<ref_result_type>;
		return requires(named_ref_type &ref, Args &...args) {
			::fast_io::operations::decay::add_output_decos_decay(
				ref, ::fast_io::freestanding::move(args)...);
		};
	}
}

template <typename T, typename... Args>
inline consteval bool has_add_input_decos() noexcept
{
	using source_type = ::std::remove_reference_t<T>;
	if constexpr (!::fast_io::operations::defines::has_input_or_io_stream_deco_filter_ref_define<source_type &>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::input_stream_deco_filter_ref(
			::std::declval<source_type &>()));
		using named_ref_type = ::std::remove_reference_t<ref_result_type>;
		return requires(named_ref_type &ref, Args &...args) {
			::fast_io::operations::decay::add_input_decos_decay(
				ref, ::fast_io::freestanding::move(args)...);
		};
	}
}

template <typename T, typename... Args>
inline consteval bool has_add_io_decos() noexcept
{
	using source_type = ::std::remove_reference_t<T>;
	if constexpr (!::fast_io::operations::defines::has_io_stream_deco_filter_ref_define<source_type &>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::io_stream_deco_filter_ref(
			::std::declval<source_type &>()));
		using named_ref_type = ::std::remove_reference_t<ref_result_type>;
		return requires(named_ref_type &ref, Args &...args) {
			::fast_io::operations::decay::add_io_decos_decay(
				ref, ::fast_io::freestanding::move(args)...);
		};
	}
}

} // namespace defines

template <typename T, typename... Args>
		requires(::fast_io::operations::defines::has_add_output_decos<T, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void add_output_decos(T &&t, Args... args)
{
	decltype(auto) ref = ::fast_io::operations::output_stream_deco_filter_ref(t);
	::fast_io::operations::decay::add_output_decos_decay(
		ref, ::fast_io::freestanding::move(args)...);
}

template <typename T, typename... Args>
		requires(::fast_io::operations::defines::has_add_input_decos<T, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void add_input_decos(T &&t, Args... args)
{
	decltype(auto) ref = ::fast_io::operations::input_stream_deco_filter_ref(t);
	::fast_io::operations::decay::add_input_decos_decay(
		ref, ::fast_io::freestanding::move(args)...);
}

template <typename T, typename... Args>
		requires(::fast_io::operations::defines::has_add_io_decos<T, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void add_io_decos(T &&t, Args... args)
{
	decltype(auto) ref = ::fast_io::operations::io_stream_deco_filter_ref(t);
	::fast_io::operations::decay::add_io_decos_decay(
		ref, ::fast_io::freestanding::move(args)...);
}

} // namespace operations

} // namespace fast_io

#pragma once

/*
 * Transcoding decorator filter construction (IO/protocol bridge).
 *
 * This file recognizes and dispatches directional stream CPOs that combine a
 * stream with a character transcoder/decorator. It establishes the filtered
 * observer and state ownership only. Encoding conversion and device transfer
 * remain delegated to the transcode and read/write operation layers.
 */

#pragma once

namespace fast_io
{

namespace operations::decay
{

namespace defines
{

template <typename T, typename P>
concept has_output_stream_transcode_deco_filter_define =
	requires(T &t, P &&p) {
		output_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
	};

template <typename T, typename P>
concept has_input_stream_transcode_deco_filter_define =
	requires(T &t, P &&p) {
		input_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
	};

template <typename T, typename P>
concept has_io_stream_transcode_deco_filter_define =
	requires(T &t, P &&p) {
		io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p));
	};

template <typename T, typename P>
concept has_output_or_io_stream_transcode_deco_filter_define =
	has_output_stream_transcode_deco_filter_define<T, P> || has_io_stream_transcode_deco_filter_define<T, P>;

template <typename T, typename P>
concept has_input_or_io_stream_transcode_deco_filter_define =
	has_input_stream_transcode_deco_filter_define<T, P> || has_io_stream_transcode_deco_filter_define<T, P>;

/// @brief Proves that the transcode CPO's result can become the returned stream owner exactly once.
/// @details A same-type prvalue is admitted without asking for a move constructor because direct initialization and
///          return use the C++17 guaranteed-elision model. A reference result is different: the public operation
///          returns by value, so materialization from that exact reference category really occurs and must be valid.
///          Completeness is tested before library type traits to keep malformed customization results in substitution.
template <typename result_type>
inline consteval bool storable_stream_transcode_deco_filter_result_object() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (!::std::is_object_v<::std::remove_reference_t<result_type>> ||
				  !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else if constexpr (!::std::is_destructible_v<result_value_type>)
	{
		return false;
	}
	else if constexpr (::std::is_reference_v<result_type>)
	{
		return ::std::constructible_from<result_value_type, result_type>;
	}
	else
	{
		return true;
	}
}

template <typename result_type>
concept storable_stream_transcode_deco_filter_result =
	storable_stream_transcode_deco_filter_result_object<result_type>();

template <typename T, typename P>
concept has_storable_output_stream_transcode_deco_filter_define =
	has_output_stream_transcode_deco_filter_define<T, P> && requires(T &t, P &&p) {
		requires storable_stream_transcode_deco_filter_result<decltype(output_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p)))>;
	};

template <typename T, typename P>
concept has_storable_input_stream_transcode_deco_filter_define =
	has_input_stream_transcode_deco_filter_define<T, P> && requires(T &t, P &&p) {
		requires storable_stream_transcode_deco_filter_result<decltype(input_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p)))>;
	};

template <typename T, typename P>
concept has_storable_io_stream_transcode_deco_filter_define =
	has_io_stream_transcode_deco_filter_define<T, P> && requires(T &t, P &&p) {
		requires storable_stream_transcode_deco_filter_result<decltype(io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<P>(p)))>;
	};

template <typename T, typename P>
concept has_storable_output_or_io_stream_transcode_deco_filter_define =
	has_storable_output_stream_transcode_deco_filter_define<T, P> ||
	has_storable_io_stream_transcode_deco_filter_define<T, P>;

template <typename T, typename P>
concept has_storable_input_or_io_stream_transcode_deco_filter_define =
	has_storable_input_stream_transcode_deco_filter_define<T, P> ||
	has_storable_io_stream_transcode_deco_filter_define<T, P>;

/// @brief Proves the post-transcode decorator chain against the exact named-reference expressions used by execution.
/// @details The transcode result is owned once as `newdecof`. Its filter-reference result is then named with
///          `decltype(auto)` and therefore becomes an lvalue expression regardless of whether the customization
///          returned a value or a stable reference. Modelling that named expression here prevents a constraint from
///          accepting a pack which later fails in the function body, without copying either owner or observer merely
///          to perform the proof.
template <typename transcode_result_type, typename... Args>
inline consteval bool has_input_transcode_decos_tail() noexcept
{
	if constexpr (sizeof...(Args) == 0)
	{
		return true;
	}
	else
	{
		using owner_type = ::std::remove_cvref_t<transcode_result_type>;
		// `newdecof` is a named local in this branch, so NRVO is permitted but not guaranteed. The return statement
		// must therefore have a valid move-or-copy construction even though the zero-tail direct-prvalue branch need
		// not impose that stronger requirement.
		if constexpr (!::std::constructible_from<owner_type, owner_type &&>)
		{
			return false;
		}
		else if constexpr (!requires(owner_type &owner) {
							   ::fast_io::operations::input_stream_deco_filter_ref(owner);
						   })
		{
			return false;
		}
		else
		{
			using ref_result_type = decltype(::fast_io::operations::input_stream_deco_filter_ref(::std::declval<owner_type &>()));
			using named_ref_type = ::std::remove_reference_t<ref_result_type>;
			return requires(named_ref_type &ref, Args &&...args) {
				::fast_io::operations::decay::add_input_decos_decay(
					ref, ::fast_io::freestanding::forward<Args>(args)...);
			};
		}
	}
}

template <typename transcode_result_type, typename... Args>
inline consteval bool has_output_transcode_decos_tail() noexcept
{
	if constexpr (sizeof...(Args) == 0)
	{
		return true;
	}
	else
	{
		using owner_type = ::std::remove_cvref_t<transcode_result_type>;
		if constexpr (!::std::constructible_from<owner_type, owner_type &&>)
		{
			return false;
		}
		else if constexpr (!requires(owner_type &owner) {
							   ::fast_io::operations::output_stream_deco_filter_ref(owner);
						   })
		{
			return false;
		}
		else
		{
			using ref_result_type = decltype(::fast_io::operations::output_stream_deco_filter_ref(::std::declval<owner_type &>()));
			using named_ref_type = ::std::remove_reference_t<ref_result_type>;
			return requires(named_ref_type &ref, Args &&...args) {
				::fast_io::operations::decay::add_output_decos_decay(
					ref, ::fast_io::freestanding::forward<Args>(args)...);
			};
		}
	}
}

template <typename transcode_result_type, typename... Args>
inline consteval bool has_io_transcode_decos_tail() noexcept
{
	if constexpr (sizeof...(Args) == 0)
	{
		return true;
	}
	else
	{
		using owner_type = ::std::remove_cvref_t<transcode_result_type>;
		if constexpr (!::std::constructible_from<owner_type, owner_type &&>)
		{
			return false;
		}
		else if constexpr (!requires(owner_type &owner) {
							   ::fast_io::operations::io_stream_deco_filter_ref(owner);
						   })
		{
			return false;
		}
		else
		{
			using ref_result_type = decltype(::fast_io::operations::io_stream_deco_filter_ref(::std::declval<owner_type &>()));
			using named_ref_type = ::std::remove_reference_t<ref_result_type>;
			return requires(named_ref_type &ref, Args &&...args) {
				::fast_io::operations::decay::add_io_decos_decay(
					ref, ::fast_io::freestanding::forward<Args>(args)...);
			};
		}
	}
}

template <typename T, typename D, typename... Args>
inline consteval bool has_input_transcode_decos_decay() noexcept
{
	if constexpr (has_storable_input_stream_transcode_deco_filter_define<T, D>)
	{
		using result_type = decltype(input_stream_transcode_deco_filter_define(
			::std::declval<T &>(), ::std::declval<D &&>()));
		return has_input_transcode_decos_tail<result_type, Args...>();
	}
	else if constexpr (has_storable_io_stream_transcode_deco_filter_define<T, D>)
	{
		using result_type = decltype(io_stream_transcode_deco_filter_define(
			::std::declval<T &>(), ::std::declval<D &&>()));
		return has_input_transcode_decos_tail<result_type, Args...>();
	}
	else
	{
		return false;
	}
}

template <typename T, typename D, typename... Args>
inline consteval bool has_output_transcode_decos_decay() noexcept
{
	if constexpr (has_storable_output_stream_transcode_deco_filter_define<T, D>)
	{
		using result_type = decltype(output_stream_transcode_deco_filter_define(
			::std::declval<T &>(), ::std::declval<D &&>()));
		return has_output_transcode_decos_tail<result_type, Args...>();
	}
	else if constexpr (has_storable_io_stream_transcode_deco_filter_define<T, D>)
	{
		using result_type = decltype(io_stream_transcode_deco_filter_define(
			::std::declval<T &>(), ::std::declval<D &&>()));
		return has_output_transcode_decos_tail<result_type, Args...>();
	}
	else
	{
		return false;
	}
}

template <typename T, typename D, typename... Args>
inline consteval bool has_io_transcode_decos_decay() noexcept
{
	if constexpr (has_storable_io_stream_transcode_deco_filter_define<T, D>)
	{
		using result_type = decltype(io_stream_transcode_deco_filter_define(
			::std::declval<T &>(), ::std::declval<D &&>()));
		return has_io_transcode_decos_tail<result_type, Args...>();
	}
	else
	{
		return false;
	}
}

} // namespace defines

/// @brief Builds an input transcode owner while borrowing one normalized source observer.
/// @details `t` already has lifetime-valid storage in the enclosing normalization operation. The decorator and every
///          trailing decorator retain their exact incoming categories because an rvalue can become owned state in the
///          returned filter. A transcode-filter customization must return a self-contained owner: it may copy or consume
///          resources denoted by `t`, but it must not retain the address of this observer object. Public normalization may
///          materialize that observer only for this construction call, irrespective of the ABI transport selected above
///          it. This is the pre-existing public construction protocol, not a new precondition introduced by the borrowed
///          helper: the historical by-value decay entry already destroyed its observer before returning the same owner.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_input_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_input_decos_decay_borrowed(T &t, D &&deco, Args &&...args)
{
	if constexpr (::fast_io::operations::decay::defines::has_storable_input_stream_transcode_deco_filter_define<T, D>)
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return input_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco));
		}
		else
		{
			auto newdecof{input_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco))};
			decltype(auto) ref = ::fast_io::operations::input_stream_deco_filter_ref(newdecof);
			::fast_io::operations::decay::add_input_decos_decay(
				ref, ::fast_io::freestanding::forward<Args>(args)...);
			return newdecof;
		}
	}
	else
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco));
		}
		else
		{
			auto newdecof{io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco))};
			decltype(auto) ref = ::fast_io::operations::input_stream_deco_filter_ref(newdecof);
			::fast_io::operations::decay::add_input_decos_decay(
				ref, ::fast_io::freestanding::forward<Args>(args)...);
			return newdecof;
		}
	}
}

/// @brief Owns one normalized input observer at the historical decay boundary.
/// @details The observer alone is transported by value. Decorator references remain forwarding expressions so their
///          source category reaches the consuming customization exactly once.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_input_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_input_decos_decay(T t, D &&deco, Args &&...args)
{
	return ::fast_io::operations::decay::transcode_input_decos_decay_borrowed(
		t, ::fast_io::freestanding::forward<D>(deco),
		::fast_io::freestanding::forward<Args>(args)...);
}

/// @brief Selects value or borrowed transport for a normalized input transcode observer.
/// @details Size and triviality cannot prove observer substitutability. The value edge is therefore instantiated only
///          when the observer author supplies the shared stream-reference semantic marker and the target ABI admits its
///          representation directly. Unmarked, stateful, and noncopyable observers keep their exact identity.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_input_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto transcode_input_decos_decay_dispatch(
	T &t, D &&deco, Args &&...args)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_stream_ref_result_object<T &>())
	{
		return ::fast_io::operations::decay::transcode_input_decos_decay(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::operations::decay::transcode_input_decos_decay_borrowed(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
}

/// @brief Builds an output transcode owner while borrowing one normalized source observer.
/// @details The same source-lifetime and exact decorator-category contract as the input counterpart applies here.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_output_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_output_decos_decay_borrowed(T &t, D &&deco, Args &&...args)
{
	if constexpr (::fast_io::operations::decay::defines::has_storable_output_stream_transcode_deco_filter_define<T, D>)
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return output_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco));
		}
		else
		{
			auto newdecof{output_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco))};
			decltype(auto) ref = ::fast_io::operations::output_stream_deco_filter_ref(newdecof);
			::fast_io::operations::decay::add_output_decos_decay(
				ref, ::fast_io::freestanding::forward<Args>(args)...);
			return newdecof;
		}
	}
	else
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco));
		}
		else
		{
			auto newdecof{io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco))};
			decltype(auto) ref = ::fast_io::operations::output_stream_deco_filter_ref(newdecof);
			::fast_io::operations::decay::add_output_decos_decay(
				ref, ::fast_io::freestanding::forward<Args>(args)...);
			return newdecof;
		}
	}
}

/// @brief Owns one normalized output observer at the historical decay boundary.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_output_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_output_decos_decay(T t, D &&deco, Args &&...args)
{
	return ::fast_io::operations::decay::transcode_output_decos_decay_borrowed(
		t, ::fast_io::freestanding::forward<D>(deco),
		::fast_io::freestanding::forward<Args>(args)...);
}

/// @brief Selects value or borrowed transport for a normalized output transcode observer.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_output_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto transcode_output_decos_decay_dispatch(
	T &t, D &&deco, Args &&...args)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_stream_ref_result_object<T &>())
	{
		return ::fast_io::operations::decay::transcode_output_decos_decay(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::operations::decay::transcode_output_decos_decay_borrowed(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
}

/// @brief Builds a bidirectional transcode owner while borrowing one normalized source observer.
/// @details The same historical construction protocol as the input path applies: the returned owner must remain
///          independent of the observer object's address. Trailing decorators remain exact forwarding expressions and
///          are attached only while the returned owner is alive locally.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_io_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_io_decos_decay_borrowed(T &t, D &&deco, Args &&...args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		return io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco));
	}
	else
	{
		auto newdecof{io_stream_transcode_deco_filter_define(t, ::fast_io::freestanding::forward<D>(deco))};
		decltype(auto) ref = ::fast_io::operations::io_stream_deco_filter_ref(newdecof);
		::fast_io::operations::decay::add_io_decos_decay(
			ref, ::fast_io::freestanding::forward<Args>(args)...);
		return newdecof;
	}
}

/// @brief Owns one normalized bidirectional observer at the historical decay boundary.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_io_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_io_decos_decay(T t, D &&deco, Args &&...args)
{
	return ::fast_io::operations::decay::transcode_io_decos_decay_borrowed(
		t, ::fast_io::freestanding::forward<D>(deco),
		::fast_io::freestanding::forward<Args>(args)...);
}

/// @brief Selects value or borrowed transport for a normalized bidirectional transcode observer.
template <typename T, typename D, typename... Args>
	requires(::fast_io::operations::decay::defines::has_io_transcode_decos_decay<T, D, Args...>())
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto transcode_io_decos_decay_dispatch(
	T &t, D &&deco, Args &&...args)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_stream_ref_result_object<T &>())
	{
		return ::fast_io::operations::decay::transcode_io_decos_decay(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::operations::decay::transcode_io_decos_decay_borrowed(
			t, ::fast_io::freestanding::forward<D>(deco),
			::fast_io::freestanding::forward<Args>(args)...);
	}
}

} // namespace operations::decay

namespace operations
{

namespace defines
{

template <typename result_type>
concept storable_stream_transcode_deco_filter_ref_result =
	::fast_io::operations::defines::storable_stream_ref_result_object<result_type>();

/// @brief Borrows only a mutable lvalue transcode projection whose storage is supplied by its CPO.
/// @details Public transcode composition names the result for the complete synchronous rebuild.  A prvalue becomes that
///          sole local owner by elision, an xvalue is moved into it, and a cv-qualified lvalue is copied only when the
///          exact construction is valid.  A mutable lvalue remains an exact reference so a large or noncopyable device
///          projection is never duplicated.  No ABI-small lvalue shortcut is used because stream identity may carry
///          buffered position state independently of byte size.
template <typename result_type>
inline constexpr bool stream_transcode_deco_filter_ref_result_borrows_lvalue =
	::std::is_lvalue_reference_v<result_type> &&
	!::std::is_const_v<::std::remove_reference_t<result_type>> &&
	!::std::is_volatile_v<::std::remove_reference_t<result_type>>;

template <typename T>
concept has_input_stream_transcode_deco_filter_ref_define =
	requires(T &&t) {
		input_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		requires ::fast_io::operations::defines::storable_stream_transcode_deco_filter_ref_result<decltype(input_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t)))>;
	};

template <typename T>
concept has_output_stream_transcode_deco_filter_ref_define =
	requires(T &&t) {
		output_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		requires ::fast_io::operations::defines::storable_stream_transcode_deco_filter_ref_result<decltype(output_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t)))>;
	};

template <typename T>
concept has_io_stream_transcode_deco_filter_ref_define =
	requires(T &&t) {
		io_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		requires ::fast_io::operations::defines::storable_stream_transcode_deco_filter_ref_result<decltype(io_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t)))>;
	};

template <typename T>
concept has_input_or_io_stream_transcode_deco_filter_ref_define =
	has_input_stream_transcode_deco_filter_ref_define<T> || has_io_stream_transcode_deco_filter_ref_define<T>;

template <typename T>
concept has_output_or_io_stream_transcode_deco_filter_ref_define =
	has_output_stream_transcode_deco_filter_ref_define<T> || has_io_stream_transcode_deco_filter_ref_define<T>;

} // namespace defines

template <typename T>
	requires(::fast_io::operations::defines::has_input_or_io_stream_transcode_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr decltype(auto) input_stream_transcode_deco_filter_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_input_stream_transcode_deco_filter_ref_define<T>)
	{
		using result_type = decltype(input_stream_transcode_deco_filter_ref_define(
			::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_transcode_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return input_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(input_stream_transcode_deco_filter_ref_define(
				::fast_io::freestanding::forward<T>(t)));
		}
	}
	else
	{
		using result_type = decltype(io_stream_transcode_deco_filter_ref_define(
			::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_transcode_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_stream_transcode_deco_filter_ref_define(
				::fast_io::freestanding::forward<T>(t)));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_output_or_io_stream_transcode_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr decltype(auto) output_stream_transcode_deco_filter_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_output_stream_transcode_deco_filter_ref_define<T>)
	{
		using result_type = decltype(output_stream_transcode_deco_filter_ref_define(
			::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_transcode_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return output_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(output_stream_transcode_deco_filter_ref_define(
				::fast_io::freestanding::forward<T>(t)));
		}
	}
	else
	{
		using result_type = decltype(io_stream_transcode_deco_filter_ref_define(
			::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_transcode_deco_filter_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_stream_transcode_deco_filter_ref_define(
				::fast_io::freestanding::forward<T>(t)));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_io_stream_transcode_deco_filter_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr decltype(auto) io_stream_transcode_deco_filter_ref(T &&t)
{
	using result_type = decltype(io_stream_transcode_deco_filter_ref_define(
		::fast_io::freestanding::forward<T>(t)));
	if constexpr (::fast_io::operations::defines::stream_transcode_deco_filter_ref_result_borrows_lvalue<result_type>)
	{
		return io_stream_transcode_deco_filter_ref_define(::fast_io::freestanding::forward<T>(t));
	}
	else
	{
		return ::std::remove_cvref_t<result_type>(io_stream_transcode_deco_filter_ref_define(
			::fast_io::freestanding::forward<T>(t)));
	}
}

namespace defines
{

/// @brief Checks an entire public transcode composition before its constrained wrapper participates in overload resolution.
/// @details The source CPO is evaluated with the same forwarded category as the public call. Its result is then named
///          exactly once; a value result becomes the sole local owner, while a non-cv lvalue result remains a stable
///          observer reference. Consequently the decay layer sees `remove_reference_t<result>&`, never a by-value
///          reconstruction. This mirrors execution and makes malformed transcode or trailing-decorator protocols a
///          false constraint instead of a diagnostic from an instantiated function body.
template <typename T, typename Deco, typename... Args>
inline consteval bool has_input_transcode_decos() noexcept
{
	if constexpr (!::fast_io::operations::defines::has_input_or_io_stream_transcode_deco_filter_ref_define<T>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::input_stream_transcode_deco_filter_ref(
			::std::declval<T &&>()));
		using borrowed_ref_type = ::std::remove_reference_t<ref_result_type>;
		return ::fast_io::operations::decay::defines::has_input_transcode_decos_decay<
			borrowed_ref_type, Deco, Args...>();
	}
}

template <typename T, typename Deco, typename... Args>
inline consteval bool has_output_transcode_decos() noexcept
{
	if constexpr (!::fast_io::operations::defines::has_output_or_io_stream_transcode_deco_filter_ref_define<T>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::output_stream_transcode_deco_filter_ref(
			::std::declval<T &&>()));
		using borrowed_ref_type = ::std::remove_reference_t<ref_result_type>;
		return ::fast_io::operations::decay::defines::has_output_transcode_decos_decay<
			borrowed_ref_type, Deco, Args...>();
	}
}

template <typename T, typename Deco, typename... Args>
inline consteval bool has_io_transcode_decos() noexcept
{
	if constexpr (!::fast_io::operations::defines::has_io_stream_transcode_deco_filter_ref_define<T>)
	{
		return false;
	}
	else
	{
		using ref_result_type = decltype(::fast_io::operations::io_stream_transcode_deco_filter_ref(
			::std::declval<T &&>()));
		using borrowed_ref_type = ::std::remove_reference_t<ref_result_type>;
		return ::fast_io::operations::decay::defines::has_io_transcode_decos_decay<
			borrowed_ref_type, Deco, Args...>();
	}
}

} // namespace defines

template <typename T, typename Deco, typename... Args>
	requires(!::std::is_lvalue_reference_v<T> &&
			 ::fast_io::operations::defines::has_input_transcode_decos<T, Deco, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_input_decos(T &&t, Deco &&d, Args &&...args)
{
	decltype(auto) ref = ::fast_io::operations::input_stream_transcode_deco_filter_ref(
		::fast_io::freestanding::forward<T>(t));
	return ::fast_io::operations::decay::transcode_input_decos_decay_dispatch(
		ref, ::fast_io::freestanding::forward<Deco>(d), ::fast_io::freestanding::forward<Args>(args)...);
}

template <typename T, typename Deco, typename... Args>
	requires(!::std::is_lvalue_reference_v<T> &&
			 ::fast_io::operations::defines::has_output_transcode_decos<T, Deco, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_output_decos(T &&t, Deco &&d, Args &&...args)
{
	decltype(auto) ref = ::fast_io::operations::output_stream_transcode_deco_filter_ref(
		::fast_io::freestanding::forward<T>(t));
	return ::fast_io::operations::decay::transcode_output_decos_decay_dispatch(
		ref, ::fast_io::freestanding::forward<Deco>(d), ::fast_io::freestanding::forward<Args>(args)...);
}

template <typename T, typename Deco, typename... Args>
	requires(!::std::is_lvalue_reference_v<T> &&
			 ::fast_io::operations::defines::has_io_transcode_decos<T, Deco, Args...>())
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif
inline constexpr auto transcode_io_decos(T &&t, Deco &&d, Args &&...args)
{
	decltype(auto) ref = ::fast_io::operations::io_stream_transcode_deco_filter_ref(
		::fast_io::freestanding::forward<T>(t));
	return ::fast_io::operations::decay::transcode_io_decos_decay_dispatch(
		ref, ::fast_io::freestanding::forward<Deco>(d), ::fast_io::freestanding::forward<Args>(args)...);
}

} // namespace operations

} // namespace fast_io

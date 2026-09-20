#pragma once

/*
 * Stream synchronization projection protocol (CPO level).
 *
 * A synchronized stream must provide a coherent pair: a mutex reference and an
 * unlocked stream reference for the same direction. This file validates that
 * pair, follows recursive projections without cycles, and exposes the complete
 * input/output/IO mutex capability consumed by operation-level critical
 * sections. Lock acquisition order and operation extent belong to print/scan
 * orchestration; the CPOs here only identify the synchronization domain and
 * unlocked observer.
 */

namespace fast_io
{

namespace operations::decay
{

namespace defines
{

template <typename T>
concept has_input_stream_mutex_ref_define = requires(T t) { input_stream_mutex_ref_define(t); };

template <typename T>
concept has_output_stream_mutex_ref_define = requires(T t) { output_stream_mutex_ref_define(t); };

template <typename T>
concept has_io_stream_mutex_ref_define = requires(T t) { io_stream_mutex_ref_define(t); };

template <typename T>
concept has_input_or_io_stream_mutex_ref_define =
	has_input_stream_mutex_ref_define<T> || has_io_stream_mutex_ref_define<T>;

template <typename T>
concept has_output_or_io_stream_mutex_ref_define =
	has_output_stream_mutex_ref_define<T> || has_io_stream_mutex_ref_define<T>;

template <typename T>
concept has_input_stream_unlocked_ref_define = requires(T t) { input_stream_unlocked_ref_define(t); };

template <typename T>
concept has_output_stream_unlocked_ref_define = requires(T t) { output_stream_unlocked_ref_define(t); };

template <typename T>
concept has_io_stream_unlocked_ref_define = requires(T t) { io_stream_unlocked_ref_define(t); };

template <typename T>
concept has_input_or_io_stream_unlocked_ref_define =
	has_input_stream_unlocked_ref_define<T> || has_io_stream_unlocked_ref_define<T>;

template <typename T>
concept has_output_or_io_stream_unlocked_ref_define =
	has_output_stream_unlocked_ref_define<T> || has_io_stream_unlocked_ref_define<T>;

} // namespace defines

template <typename T>
	requires(::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
/// The locked observer has a stable lifetime for the calling operation; it may be caller-owned or a normalized value.
/// Borrowing it here prevents synchronization protocol discovery from adding an unrelated observer copy. Only the
/// exact mutex CPO decides whether its own result is a value.
inline constexpr decltype(auto) output_stream_mutex_ref_decay(T &t)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_stream_mutex_ref_define<T>)
	{
		return output_stream_mutex_ref_define(t);
	}
	else
	{
		return io_stream_mutex_ref_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) input_stream_mutex_ref_decay(T &t)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_stream_mutex_ref_define<T>)
	{
		return input_stream_mutex_ref_define(t);
	}
	else
	{
		return io_stream_mutex_ref_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::has_io_stream_mutex_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) io_stream_mutex_ref_decay(T &t)
{
	return io_stream_mutex_ref_define(t);
}

template <typename T>
	requires(::fast_io::operations::decay::defines::has_output_or_io_stream_unlocked_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) output_stream_unlocked_ref_decay(T &t)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_stream_unlocked_ref_define<T>)
	{
		return output_stream_unlocked_ref_define(t);
	}
	else
	{
		return io_stream_unlocked_ref_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::has_input_or_io_stream_unlocked_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) input_stream_unlocked_ref_decay(T &t)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_stream_unlocked_ref_define<T>)
	{
		return input_stream_unlocked_ref_define(t);
	}
	else
	{
		return io_stream_unlocked_ref_define(t);
	}
}

template <typename T>
	requires(::fast_io::operations::decay::defines::has_io_stream_unlocked_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) io_stream_unlocked_ref_decay(T &t)
{
	return io_stream_unlocked_ref_define(t);
}

namespace defines
{

/// @brief Proves both owning constructions performed by mutex guard normalization.
/// @details The exact CPO result first initializes a by-value guard parameter; that parameter is then moved into the
///          guard member. A move-only prvalue consequently remains valid, while a noncopyable lvalue does not. The
///          complete-object branch is intentionally evaluated before either constructibility trait so a forward-
///          declared mutex proxy yields `false` during constraint substitution rather than a library-trait diagnostic.
template <typename result_type>
inline consteval bool storable_mutex_ref_result_object() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (!::std::is_object_v<::std::remove_reference_t<result_type>> ||
				  !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<result_value_type, result_type> &&
			   ::std::constructible_from<result_value_type, result_value_type &&>;
	}
}

/// @brief Proves the two constructions performed by `stream_ref_decay_lock_guard`.
/// @details CTAD first initializes the guard's by-value parameter from the exact mutex CPO result; guard storage is
///          then initialized from that parameter as an rvalue. This admits move-only prvalue proxies without weakening
///          stable-reference behavior: an lvalue proxy result must still be copyable into the parameter. Requiring an
///          object also rejects `void` and function-shaped customizations before member lookup.
template <typename result_type>
concept storable_mutex_ref_result =
	::fast_io::operations::decay::defines::storable_mutex_ref_result_object<result_type>() &&
	requires(::std::remove_cvref_t<result_type> &stored_proxy) {
		{ stored_proxy.lock() } -> ::std::same_as<void>;
		{ stored_proxy.unlock() } -> ::std::same_as<void>;
	};

/// @brief Recognizes a complete, type-progressing input synchronization protocol.
/// @details A mutex customization is only half of the input protocol. Every locked scan/read path also asks for an
///          unlocked reference and recursively dispatches through that reference while the guard is alive. Testing
///          the mutex marker alone therefore accepts a type for which the selected operation is necessarily
///          ill-formed.
///
///          The storage requirements below mirror the expressions used by dispatch, rather than imposing generic
///          copyability. The mutex result initializes a by-value guard parameter from its exact category and that
///          parameter is then moved into stable storage; consequently move-only prvalues work while a stable lvalue
///          proxy must remain copyable. Recursive high-level dispatch establishes the unlocked result once: a prvalue
///          is materialized, while an lvalue keeps its existing identity. The result is then borrowed, so admission
///          uses the one-owner `storable_input_stream_ref_result` contract rather than the explicit value-transport
///          refinement. A lower primitive that intentionally copies an observer must prove that refinement at its own
///          boundary. Exact `void` lock/unlock results rule out similarly named queries that cannot satisfy the RAII
///          contract.
///
///          Unwrapping must preserve `input_char_type`: the caller has already formed character pointers, buffer
///          descriptors, or scanner state for the locked stream's character domain. A different character type would
///          invalidate that interpretation. Finally, the normalized unlocked type must differ from the normalized
///          locked type. This excludes immediate self-unwrapping, the recursion failure a single customization can
///          introduce without involving a second protocol type; it does not claim to detect an arbitrary multi-type
///          cycle, for which the current CPO vocabulary carries no global normalization identity.
template <typename T>
concept has_locally_complete_input_stream_mutex_protocol =
	::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<T> &&
	::fast_io::operations::decay::defines::has_input_or_io_stream_unlocked_ref_define<T> &&
	requires(T instm) {
		typename ::std::remove_cvref_t<T>::input_char_type;
		typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm))>::input_char_type;
		requires ::fast_io::operations::decay::defines::storable_mutex_ref_result<decltype(::fast_io::operations::decay::input_stream_mutex_ref_decay(instm))>;
		requires ::fast_io::operations::defines::storable_input_stream_ref_result<decltype(::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm))>;
		requires ::std::same_as<
			typename ::std::remove_cvref_t<T>::input_char_type,
			typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm))>::input_char_type>;
		requires(!::std::same_as<
				 ::std::remove_cvref_t<T>,
				 ::std::remove_cvref_t<decltype(::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm))>>);
	};

/// @brief Recognizes a complete, type-progressing output synchronization protocol.
/// @details A mutex customization is not an independent capability: every locked output operation must also obtain an
///          unlocked reference while the guard is alive. Detecting only `*_stream_mutex_ref_define` admits a partial
///          protocol and moves the failure into the selected print/write body. Guard admission follows the exact CPO
///          result into a by-value parameter and then into storage from an rvalue parameter, admitting move-only
///          prvalues without pretending that a noncopyable lvalue proxy can be captured. Requiring exact `void`
///          lock/unlock results records the RAII contract rather than accepting an unrelated query with the same name.
///
///          The unlocked reference must satisfy one-time output-observer materialization, preserve the output character
///          type, and have a different normalized type. Character preservation proves that arguments
///          forwarded for the locked stream remain valid after unwrapping. Requiring a different immediate type rejects
///          direct self-recursion. It is intentionally a local invariant, not a proof that independently authored
///          wrapper types cannot form a longer normalization cycle.
template <typename T>
concept has_locally_complete_output_stream_mutex_protocol =
	::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<T> &&
	::fast_io::operations::decay::defines::has_output_or_io_stream_unlocked_ref_define<T> &&
	requires(T outstm) {
		typename ::std::remove_cvref_t<T>::output_char_type;
		typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::output_stream_unlocked_ref_decay(outstm))>::output_char_type;
		requires ::fast_io::operations::decay::defines::storable_mutex_ref_result<decltype(::fast_io::operations::decay::output_stream_mutex_ref_decay(outstm))>;
		requires ::fast_io::operations::defines::storable_output_stream_ref_result<decltype(::fast_io::operations::decay::output_stream_unlocked_ref_decay(outstm))>;
		requires ::std::same_as<
			typename ::std::remove_cvref_t<T>::output_char_type,
			typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::output_stream_unlocked_ref_decay(outstm))>::output_char_type>;
		requires(!::std::same_as<
				 ::std::remove_cvref_t<T>,
				 ::std::remove_cvref_t<decltype(::fast_io::operations::decay::output_stream_unlocked_ref_decay(outstm))>>);
	};

/// @brief Recognizes a complete synchronization protocol for an operation that jointly mutates input and output
///        state.
/// @details An io-level operation cannot be implemented by independently selecting the input and output protocols:
///          those protocols may name different mutexes or different unlocked objects. Requiring the io-specific CPOs
///          proves that one guard protects both directions and that recursion continues through one coherent io
///          reference. The guard-storage and exact `void` requirements are the same executable-expression proof used
///          by the directional protocols.
///
///          The unlocked result must satisfy joint one-time observer materialization. Both character domains are preserved
///          because an io seek or flush may consume pending input and output buffer state in the same call. A different
///          normalized type is required so the immediately recursive dispatch cannot select the same mutex branch
///          again. This is the local termination invariant supplied by every supported synchronization layer;
///          operation-specific admission additionally rejects a partial marker before a function body is selected.
template <typename T>
concept has_locally_complete_io_stream_mutex_protocol =
	::fast_io::operations::decay::defines::has_io_stream_mutex_ref_define<T> &&
	::fast_io::operations::decay::defines::has_io_stream_unlocked_ref_define<T> &&
	requires(T iostm) {
		typename ::std::remove_cvref_t<T>::input_char_type;
		typename ::std::remove_cvref_t<T>::output_char_type;
		typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>::input_char_type;
		typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>::output_char_type;
		requires ::fast_io::operations::decay::defines::storable_mutex_ref_result<decltype(::fast_io::operations::decay::io_stream_mutex_ref_decay(iostm))>;
		requires ::fast_io::operations::defines::storable_io_stream_ref_result<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>;
		requires ::std::same_as<
			typename ::std::remove_cvref_t<T>::input_char_type,
			typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>::input_char_type>;
		requires ::std::same_as<
			typename ::std::remove_cvref_t<T>::output_char_type,
			typename ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>::output_char_type>;
		requires(!::std::same_as<
				 ::std::remove_cvref_t<T>,
				 ::std::remove_cvref_t<decltype(::fast_io::operations::decay::io_stream_unlocked_ref_decay(iostm))>>);
	};

namespace mutex_protocol_details
{

/// @brief Reports whether normalization has already visited the same stream wrapper identity.
/// @details Capability discovery itself preserves cv-qualification because a const observer may expose a different CPO
///          set. The visited set has the narrower termination role: local protocol admission already forbids progress
///          which changes only cv/ref spelling, so comparing `remove_cvref_t` identities detects every permitted cycle
///          without inventing mutability for the next executable edge.
template <typename T, typename... visited_types>
inline constexpr bool type_was_visited =
	(::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<visited_types>> || ...);

/// @brief Proves that input mutex normalization reaches an unlocked terminal without repeating a wrapper type.
/// @details The input CPO precedence makes normalization a deterministic type chain. Every locally complete edge owns
///          a valid guard, produces a storable observer, and preserves the input character domain. If the next type has
///          no input/io mutex marker, primitive dispatch has reached its terminal and the proof succeeds. Otherwise
///          recursion records the current identity before examining the next edge. A repeated identity would replay
///          the same deterministic suffix forever, so rejecting it is both necessary and sufficient for finite-chain
///          cycle detection. The visited check precedes all local traits, which also prevents a cycle from recursively
///          instantiating completeness or constructibility queries without bound.
template <typename T, typename... visited_types>
inline consteval bool has_complete_input_stream_mutex_protocol_chain() noexcept
{
	using execution_type = ::std::remove_reference_t<T>;
	if constexpr (type_was_visited<execution_type, visited_types...>)
	{
		return false;
	}
	else if constexpr (!::fast_io::operations::decay::defines::has_locally_complete_input_stream_mutex_protocol<
						   execution_type>)
	{
		return false;
	}
	else
	{
		// Runtime binds this result with `decltype(auto)`. Removing only its reference reproduces the cv-qualified
		// template argument used by the next recursive dispatcher instead of probing a mutable surrogate.
		using unlocked_type = ::std::remove_reference_t<decltype(
			::fast_io::operations::decay::input_stream_unlocked_ref_decay(
				::std::declval<execution_type &>()))>;
		if constexpr (!::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<
						  unlocked_type>)
		{
			return true;
		}
		else
		{
			return has_complete_input_stream_mutex_protocol_chain<
				unlocked_type, visited_types..., execution_type>();
		}
	}
}

/// @brief Output-direction counterpart of `has_complete_input_stream_mutex_protocol_chain`.
/// @details Output-specific marker selection is deliberate: an input-only lock on an otherwise bidirectional adapter
///          does not participate in print/write recursion. Each accepted edge separately proves output character-domain
///          preservation, so an acyclic chain preserves that domain transitively.
template <typename T, typename... visited_types>
inline consteval bool has_complete_output_stream_mutex_protocol_chain() noexcept
{
	using execution_type = ::std::remove_reference_t<T>;
	if constexpr (type_was_visited<execution_type, visited_types...>)
	{
		return false;
	}
	else if constexpr (!::fast_io::operations::decay::defines::has_locally_complete_output_stream_mutex_protocol<
						   execution_type>)
	{
		return false;
	}
	else
	{
		// Preserve the constness of the named unlocked result exactly as output execution does at this edge.
		using unlocked_type = ::std::remove_reference_t<decltype(
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(
				::std::declval<execution_type &>()))>;
		if constexpr (!::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<
						  unlocked_type>)
		{
			return true;
		}
		else
		{
			return has_complete_output_stream_mutex_protocol_chain<
				unlocked_type, visited_types..., execution_type>();
		}
	}
}

/// @brief Joint-direction counterpart used by operations that require one coherent io lock chain.
/// @details Only io-specific markers continue this chain. Falling back to independently selected input and output
///          mutexes would not prove that one guard protects shared buffer/seek state. Local edge admission preserves
///          both character domains; the visited set rejects a repeated normalized type in the deterministic chain.
///          This is a cycle check, not a proof that an unbounded chain of distinct generated types will terminate.
template <typename T, typename... visited_types>
inline consteval bool has_complete_io_stream_mutex_protocol_chain() noexcept
{
	using execution_type = ::std::remove_reference_t<T>;
	if constexpr (type_was_visited<execution_type, visited_types...>)
	{
		return false;
	}
	else if constexpr (!::fast_io::operations::decay::defines::has_locally_complete_io_stream_mutex_protocol<
						   execution_type>)
	{
		return false;
	}
	else
	{
		// Joint recursion has the same named-result rule; both directional capabilities belong to this exact cv type.
		using unlocked_type = ::std::remove_reference_t<decltype(
			::fast_io::operations::decay::io_stream_unlocked_ref_decay(
				::std::declval<execution_type &>()))>;
		if constexpr (!::fast_io::operations::decay::defines::has_io_stream_mutex_ref_define<unlocked_type>)
		{
			return true;
		}
		else
		{
			return has_complete_io_stream_mutex_protocol_chain<
				unlocked_type, visited_types..., execution_type>();
		}
	}
}

} // namespace mutex_protocol_details

/// @brief Recognizes an input synchronization protocol chain that reaches a marker-free terminal.
/// @details Local CPO/storage/character checks are composed with a normalized-type visited set. Consequently a wrapper
///          that is locally valid but participates in `A -> B -> A` is rejected before an operation body can
///          instantiate recursively. Finite acyclic nesting remains valid. The compile-time visited set itself adds no
///          run-time state or branch; it does not make an infinite sequence of distinct wrapper types a finite proof.
template <typename T>
concept has_complete_input_stream_mutex_protocol =
	::fast_io::operations::decay::defines::mutex_protocol_details::
		has_complete_input_stream_mutex_protocol_chain<T>();

/// @brief Recognizes an output synchronization protocol chain that reaches a marker-free terminal.
template <typename T>
concept has_complete_output_stream_mutex_protocol =
	::fast_io::operations::decay::defines::mutex_protocol_details::
		has_complete_output_stream_mutex_protocol_chain<T>();

/// @brief Recognizes a joint io synchronization protocol chain that reaches a marker-free terminal.
template <typename T>
concept has_complete_io_stream_mutex_protocol =
	::fast_io::operations::decay::defines::mutex_protocol_details::
		has_complete_io_stream_mutex_protocol_chain<T>();

} // namespace defines

} // namespace operations::decay
} // namespace fast_io

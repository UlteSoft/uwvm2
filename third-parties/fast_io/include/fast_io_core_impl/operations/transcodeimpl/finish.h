#pragma once

/**
 * @file
 * @brief Defines explicit terminal operations for transcoder stream adapters.
 *
 * Output finish commits trailers/tags and closes the output direction. Input
 * drain-and-finish is intentionally named differently: it discards unread
 * decoded data while consuming physical EOF to force terminal validation.
 * Ordinary scan/read never invokes that destructive input operation.
 */

namespace fast_io::operations::decay::defines
{

/** @brief Detects terminal output-finish support on a normalized observer. */
template <typename T>
concept has_output_stream_finish_define = requires(T ref) {
	{ output_stream_finish_define(ref) } -> ::std::same_as<void>;
};

/** @brief Detects explicit destructive input drain-and-finish support. */
template <typename T>
concept has_input_stream_drain_and_finish_define = requires(T ref) {
	{ input_stream_drain_and_finish_define(ref) } -> ::std::same_as<void>;
};

} // namespace fast_io::operations::decay::defines

namespace fast_io::operations::decay
{

/** @brief Dispatches terminal output finish through one stable observer borrow. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_output_stream_finish_define<::std::remove_cvref_t<T>>
	inline void output_stream_finish_decay_borrowed(T &ref)
{
	output_stream_finish_define(ref);
}

/** @brief Owns a normalized output observer at the explicit decay boundary. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_output_stream_finish_define<::std::remove_cvref_t<T>>
	inline void output_stream_finish_decay(T ref)
{
	::fast_io::operations::decay::output_stream_finish_decay_borrowed(ref);
}

/** @brief Selects terminal output transport from the semantic and target-ABI proofs. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_output_stream_finish_define<::std::remove_cvref_t<T>>
	FAST_IO_GNU_ALWAYS_INLINE inline void output_stream_finish_decay_dispatch(T &ref)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_output_stream_ref_result<T &>)
	{
		::fast_io::operations::decay::output_stream_finish_decay(ref);
	}
	else
	{
		// Inline cursors and noncopyable observers retain the exact object returned
		// by stream normalization; terminal validation may mutate that identity.
		::fast_io::operations::decay::output_stream_finish_decay_borrowed(ref);
	}
}

/** @brief Dispatches destructive input drain-and-finish through one stable borrow. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_input_stream_drain_and_finish_define<::std::remove_cvref_t<T>>
	inline void input_stream_drain_and_finish_decay_borrowed(T &ref)
{
	input_stream_drain_and_finish_define(ref);
}

/** @brief Owns a normalized input observer at the explicit decay boundary. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_input_stream_drain_and_finish_define<::std::remove_cvref_t<T>>
	inline void input_stream_drain_and_finish_decay(T ref)
{
	::fast_io::operations::decay::input_stream_drain_and_finish_decay_borrowed(ref);
}

/** @brief Selects destructive input transport without weakening cursor identity. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_input_stream_drain_and_finish_define<::std::remove_cvref_t<T>>
	FAST_IO_GNU_ALWAYS_INLINE inline void
	input_stream_drain_and_finish_decay_dispatch(T &ref)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_input_stream_ref_result<T &>)
	{
		::fast_io::operations::decay::input_stream_drain_and_finish_decay(ref);
	}
	else
	{
		::fast_io::operations::decay::input_stream_drain_and_finish_decay_borrowed(ref);
	}
}

} // namespace fast_io::operations::decay

namespace fast_io::operations
{

/** @brief Normalizes a stream once and terminally finishes its output direction. */
template <typename T>
inline void output_stream_finish(T &&stream)
{
	// Normalize once and dispatch on the stable borrowed observer. Lifetime-
	// based automatic finish is owned by public output-operation guards instead.
	decltype(auto) ref{
		::fast_io::operations::output_stream_ref(stream)};
	::fast_io::operations::decay::output_stream_finish_decay_dispatch(ref);
}

/**
 * @brief Consumes physical EOF and terminally validates an input transcoder.
 *
 * Unlike ordinary finite reads, this operation is explicitly destructive: it
 * discards all remaining decoded output in order to drive the engine to EOF.
 */
template <typename T>
inline void input_stream_drain_and_finish(T &&stream)
{
	// This explicit API is the caller's assertion that the remainder of the
	// logical message may be discarded in exchange for complete validation.
	decltype(auto) ref{
		::fast_io::operations::input_stream_ref(stream)};
	::fast_io::operations::decay::input_stream_drain_and_finish_decay_dispatch(ref);
}

} // namespace fast_io::operations

namespace fast_io
{

using ::fast_io::operations::output_stream_finish;
using ::fast_io::operations::input_stream_drain_and_finish;

/** @brief Delegates explicit drain-and-finish to an input adapter owner. */
template <typename owner>
inline void input_stream_drain_and_finish_define(
	::fast_io::basic_itranscoder_ref<owner> ref)
{
	ref.ptr->drain_and_finish();
}

/** @brief Finishes only the input child of a duplex transcoder owner. */
template <typename owner>
inline void input_stream_drain_and_finish_define(
	::fast_io::basic_iotranscoder_ref<owner> ref)
{
	ref.ptr->drain_and_finish_input();
}

} // namespace fast_io

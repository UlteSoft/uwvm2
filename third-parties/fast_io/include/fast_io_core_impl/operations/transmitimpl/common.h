#pragma once

/*
 * Shared admission, storage, and progress vocabulary for transmit (IO level).
 *
 * This file validates the two directional stream references and their complete
 * mutex projections, defines transfer-count adaptation, and selects bounded
 * fallback buffer sizes. Public transmit operations own each normalized
 * observer once; every recursive or buffered continuation borrows those stable
 * objects.
 */

namespace fast_io
{

namespace operations::decay::defines
{

/*
 * Observer transport invariant shared by every transmit strategy:
 *
 * Public transmit functions retain each stream-ref result in a `decltype(auto)`
 * local. A prvalue therefore has exactly one normalization owner, while a
 * mutable CPO lvalue keeps its original identity. Let Vout and Vin denote the corresponding
 * `abi_value_{output,input}_stream_ref_result<T &>` propositions, and define
 *
 *     P(V, T) = T  if V is true, otherwise T &.
 *
 * The mandatory-inline dispatch boundary selects
 * `operation(P(Vout, O), P(Vin, I), ...)`. Thus the two directions are
 * independent: an identity-sensitive input cannot force a proven output proxy
 * back to reference transport, or vice versa. Each V proposition includes both
 * the explicit ADL `stream_ref_value_transport_safe_define` substitution proof
 * and the target ABI argument policy; size or triviality alone is insufficient.
 *
 * The selected transport entry immediately borrows its named parameters into
 * one recursive implementation. Mutex recursion also stores every unlocked CPO
 * result once and continues through that borrowed implementation. Consequently
 * inline cursors, noncopyable observers, and immovable prvalue owners retain
 * exact identity, while explicitly substitutable small proxies can still cross
 * an outlined boundary as true value parameters. Final register allocation
 * remains the compiler's ABI decision. The historical unsuffixed `*_decay`
 * functions remain separate true by-value owner entries.
 */

/// @brief Maps one independently proved transmit direction to its ABI parameter form.
template <bool value_transport, typename observer>
using transmit_stream_transport_parameter =
	::std::conditional_t<value_transport, observer, observer &>;

/// @brief Requires every synchronization marker participating in a transmit to provide its complete directional
///        protocol.
/// @details Transmit recursively replaces a locked stream with its unlocked reference. A marker without the matching
///          unlocked CPO, a storable exact-void mutex proxy, character preservation, or immediate type progress can
///          never satisfy that recursion. Rejecting the pair in constraints keeps such partial protocols out of every
///          transmit body instead of allowing a later, operation-dependent substitution failure.
///
///          This concept deliberately does not claim that two independent mutex proxies have a globally safe lock
///          order. The current mutex vocabulary exposes only `lock()` and `unlock()`; it provides neither comparable
///          identity nor `try_lock()`, so deadlock-free dual acquisition cannot be proved at the concept layer.
template <typename output, typename input>
concept has_complete_transmit_mutex_protocols =
	(!::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<
		 ::std::remove_cvref_t<output>> ||
	 ::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
		 ::std::remove_cvref_t<output>>) &&
	(!::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<
		 ::std::remove_cvref_t<input>> ||
	 ::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<::std::remove_cvref_t<input>>);

} // namespace operations::decay::defines

namespace operations::decay
{

/**
 * @brief Materializes the independently selected stream pair for a counted transmit.
 * @details The non-type operation denotes the single borrowed implementation. Value
 *          parameters become named owners here and are never copied again.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input>
inline constexpr decltype(auto) transmit_stream_pair_count_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream,
	::fast_io::uintfpos_t count)
{
	return borrowed_operation(output_stream, input_stream, count);
}

/** @brief Selects the two counted-transmit parameter forms at the mandatory-inline boundary. */
template <auto borrowed_operation, typename output, typename input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_count_dispatch(output &output_stream, input &input_stream,
									::fast_io::uintfpos_t count)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input>(output_stream, input_stream, count);
}

/**
 * @brief Materializes the independently selected stream pair for an unbounded transmit.
 * @details This overload carries no operation state; both parameter decisions remain
 *          visible in the specialized function type and therefore in its target ABI.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input>
inline constexpr decltype(auto) transmit_stream_pair_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream)
{
	return borrowed_operation(output_stream, input_stream);
}

/** @brief Selects the two unbounded-transmit parameter forms at the mandatory-inline boundary. */
template <auto borrowed_operation, typename output, typename input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_dispatch(output &output_stream, input &input_stream)
{
	return ::fast_io::operations::decay::transmit_stream_pair_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input>(output_stream, input_stream);
}

/**
 * @brief Materializes a selected stream pair while borrowing one progress accumulator.
 * @details Progress adaptation has no stream-ref substitution marker. Its owner is
 *          therefore created only by the historical/public value boundary and every
 *          recursive continuation observes that same named accumulator by reference.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input,
		  typename accumulator>
inline constexpr decltype(auto) transmit_stream_pair_accumulator_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream,
	accumulator &result)
{
	return borrowed_operation(output_stream, input_stream, result);
}

/** @brief Selects both stream forms without changing the accumulator's identity. */
template <auto borrowed_operation, typename output, typename input,
		  typename accumulator>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_accumulator_dispatch(output &output_stream,
										  input &input_stream,
										  accumulator &result)
{
	return ::fast_io::operations::decay::transmit_stream_pair_accumulator_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input, accumulator>(output_stream, input_stream,
														result);
}

} // namespace operations::decay

namespace details
{

template <typename T>
concept transmit_integer_wrapper = requires(T t, ::std::size_t off, ::fast_io::uintfpos_t uoff) {
	transmit_integer_add_define(t, off);
	transmit_integer_assign_from_uintfpos_define(t, uoff);
};

/// @brief Converts transmit's configured byte budget into a nonzero endpoint-element count.
/// @details `sz` is the size of one typed endpoint element (or one for byte transmit).  The configured budget must hold
///          at least one complete element; the former reversed comparison rejected every ordinary budget larger than
///          `sz` and admitted configurations whose integer division produced zero elements.  This relation matches the
///          generic buffer and transcoder policies: validate bytes first, then perform exactly one unit conversion.
template <::std::size_t sz>
inline constexpr ::std::size_t calculate_transmit_buffer_size() noexcept
{
	static_assert(sz != 0u);
#ifdef FAST_IO_BUFFER_SIZE
	static_assert(sz <= FAST_IO_BUFFER_SIZE);
	static_assert(FAST_IO_BUFFER_SIZE < SIZE_MAX);
	return FAST_IO_BUFFER_SIZE / sz;
#else
	if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least16_t))
	{
		return 4096 / sz;
	}
	else
	{
		return 131072 / sz;
	}
#endif
}

template <::std::size_t sz>
inline constexpr ::std::size_t transmit_buffer_size_cache{calculate_transmit_buffer_size<sz>()};

} // namespace details

struct uintfpos_transmit_reference_wrapper
{
	::fast_io::uintfpos_t *pfpos{};
};

inline constexpr void transmit_integer_assign_from_uintfpos_define(uintfpos_transmit_reference_wrapper t,
																   ::std::size_t off) noexcept
{
	*t.pfpos = off;
}

inline constexpr void transmit_integer_add_define(uintfpos_transmit_reference_wrapper t, ::std::size_t off) noexcept
{
	using commontype = ::std::common_type_t<::fast_io::uintfpos_t, ::std::size_t>;
	commontype val{static_cast<commontype>(*t.pfpos)};
	constexpr commontype mx{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
	if constexpr (sizeof(::fast_io::uintfpos_t) < sizeof(::std::size_t))
	{
		if (mx < off)
		{
			*t.pfpos = mx;
			return;
		}
	}
	commontype mxval{static_cast<commontype>(static_cast<commontype>(mx) - off)};
	if (mxval < val)
	{
		*t.pfpos = mx;
	}
	else
	{
		*t.pfpos = val + off;
	}
}

struct transmit_result
{
	::fast_io::uintfpos_t transmitted{};
	inline constexpr bool is_overflowed() noexcept
	{
		constexpr auto mxval{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
		return transmitted == mxval;
	}
};

/// @feature concept:runtime_precise_size
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, transmit_result>)
{
	constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, ::fast_io::uintfpos_t>) + 1};
	return sz;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(::fast_io::io_reserve_type_t<char_type, transmit_result>,
												 char_type *iter, transmit_result r)
{
	::fast_io::uintfpos_t transmittedsz{r.transmitted};
	constexpr auto mxval{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
	iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::fast_io::uintfpos_t>, iter, transmittedsz);
	if (transmittedsz == mxval)
	{
		*iter = ::fast_io::char_literal_v<u8'+', char_type>;
		++iter;
	}
	return iter;
}

} // namespace fast_io

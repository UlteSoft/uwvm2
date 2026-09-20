#pragma once

/*
 * Bounded partial byte transmit operation (IO level).
 *
 * This byte-domain counterpart moves up to a requested byte count and returns
 * the actual progress, preferring a provider whole-operation CPO before the
 * generic normalized byte read/write loop. The payload remains opaque.
 * An opted-in temporary output is checked-finished after the reported partial
 * transfer succeeds, while the input remains at its returned progress point.
 */

namespace fast_io
{

namespace details
{

/** @brief Copies up to a requested byte count and reports actual progress. */
template <typename optstmtype, typename instmtype>
inline constexpr ::fast_io::uintfpos_t transmit_bytes_some_main_impl(optstmtype &optstm, instmtype &instm,
																	 ::fast_io::uintfpos_t needtransmit)
{
	if (needtransmit == 0u)
	{
		/*
		The closed interval [0, 0] has the unique progress result zero.  Placing
		this proof at the terminal layer retains outer reference, lock, and finish
		protocols while removing the default 128 KiB staging allocation and all
		primitive byte I/O from the identity case.
		*/
		return 0u;
	}
	// Generic partial transfer stages through one bounded local byte buffer and
	// reports only bytes that have also been committed to the output.
	constexpr ::std::size_t default_buffer_bytes{
		::fast_io::details::transmit_buffer_size_cache<1>};
	::std::size_t buffer_bytes{default_buffer_bytes};
	if (needtransmit < static_cast<::fast_io::uintfpos_t>(default_buffer_bytes))
	{
		/*
		The partial bound and the staging capacity are both bytes, independent
		of input_char_type. The comparison is performed before narrowing, and its
		true branch proves the request fits size_t. The resulting allocation is
		therefore min(default_buffer_bytes, remaining_request) bytes.
		*/
		buffer_bytes = static_cast<::std::size_t>(needtransmit);
	}
	::fast_io::details::local_operator_new_array_ptr<::std::byte> newptr(
		buffer_bytes);
	::std::byte *buffer_start{newptr.ptr};
	::fast_io::uintfpos_t totransmit{needtransmit};
	while (totransmit)
	{
		// Attempt one bounded byte block and stop on the first short EOF result.
		::std::size_t this_round{buffer_bytes};
		if (totransmit < this_round)
		{
			// Bound the current read by the caller's remaining byte request.
			this_round = static_cast<::std::size_t>(totransmit);
		}
		::std::byte *iter{
			::fast_io::operations::decay::read_some_bytes_decay_dispatch(instm, buffer_start, buffer_start + this_round)};
		if (iter == buffer_start)
		{
			// A zero-length read ends this partial transfer without forcing EOF.
			break;
		}
		::fast_io::operations::decay::write_all_bytes_decay_dispatch(optstm, buffer_start, iter);
		totransmit -= static_cast<::std::size_t>(iter - buffer_start);
	}
	return needtransmit - totransmit;
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying partial-byte mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_some_decay_borrowed(optstmtype &optstm, instmtype &instm,
																   ::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_some_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_some_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Hold the output mutex for the whole partial-transfer operation.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_some_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock input only when output already needs no recursive unwrapping.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_some_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Execute the generic partial byte loop on unlocked observers.
		return ::fast_io::details::transmit_bytes_some_main_impl(optstm, instm, totransmit);
	}
}

/** @brief Owns both observers at the historical partial-byte value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_some_decay(optstmtype optstm, instmtype instm,
														  ::fast_io::uintfpos_t totransmit)
{
	// This signature, unlike a forwarding reference, gives small explicit owners
	// their native aggregate argument class before the borrowed algorithm begins.
	return ::fast_io::operations::decay::transmit_bytes_some_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed byte-stream transport. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_bytes_some_decay_dispatch(optstmtype &optstm, instmtype &instm,
								   ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_bytes_some_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/** @brief Partially transfers bytes and checked-finishes an eligible output. */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_bytes_some(optstmtype &&optstm, instmtype &&instm,
													::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_bytes_some_decay_dispatch(output_observer, input_observer,
																				totransmit);
	});
}

} // namespace operations

} // namespace fast_io

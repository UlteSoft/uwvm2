#pragma once

/*
 * Exact-count byte transmit operation (IO level).
 *
 * This byte-domain counterpart prefers a provider
 * `status_transmit_bytes_all_define` CPO and otherwise copies through the
 * normalized byte read/write primitives until the requested count completes.
 * Character interpretation is deliberately absent.
 * Checked output finish is applied only after the exact-count operation
 * succeeds; a finite transfer makes no terminal claim about its input.
 */

namespace fast_io
{

namespace details
{

/** @brief Copies an exact byte count through a bounded temporary buffer. */
template <typename optstmtype, typename instmtype>
inline constexpr void transmit_bytes_all_main_impl(optstmtype &optstm, instmtype &instm,
												   ::fast_io::uintfpos_t totransmit)
{
	if (totransmit == 0u)
	{
		/*
		Zero bytes form the identity element of exact byte transfer.  The shortcut
		is terminal rather than public so observer normalization, mutex recursion,
		and output finalization retain their contracts; only the otherwise
		unobservable staging allocation and primitive I/O are eliminated.
		*/
		return;
	}
	// Generic byte transfer intentionally stages through a bounded local
	// buffer; provider-specific zero-copy belongs to status transmit CPOs.
	constexpr ::std::size_t default_buffer_bytes{
		::fast_io::details::transmit_buffer_size_cache<1>};
	::std::size_t buffer_bytes{default_buffer_bytes};
	if (totransmit < static_cast<::fast_io::uintfpos_t>(default_buffer_bytes))
	{
		/*
		Both values are byte counts: this operation never derives capacity from
		the advertised stream character type. The uintfpos_t comparison precedes
		the narrowing conversion and proves it safe, making the allocation exactly
		min(default_buffer_bytes, remaining_request) bytes.
		*/
		buffer_bytes = static_cast<::std::size_t>(totransmit);
	}
	::fast_io::details::local_operator_new_array_ptr<::std::byte> newptr(
		buffer_bytes);
	::std::byte *buffer_start{newptr.ptr};
	while (totransmit)
	{
		// Move one bounded byte block while preserving the exact remaining count.
		::std::size_t this_round{buffer_bytes};
		if (totransmit < this_round)
		{
			// Limit the final iteration to the exact remaining byte count.
			this_round = static_cast<::std::size_t>(totransmit);
		}
		auto iter{buffer_start + this_round};
		::fast_io::operations::decay::read_all_bytes_decay_dispatch(instm, buffer_start, iter);
		::fast_io::operations::decay::write_all_bytes_decay_dispatch(optstm, buffer_start, iter);
		totransmit -= this_round;
	}
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying exact-byte mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_all_decay_borrowed(optstmtype &optstm, instmtype &instm,
																  ::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_all_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_all_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock and unwrap the output for the complete logical byte transfer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input only after output mutex handling is complete.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Execute the generic byte loop once both observers are unlocked.
		return ::fast_io::details::transmit_bytes_all_main_impl(optstm, instm, totransmit);
	}
}

/** @brief Owns both normalized observers at the historical byte-value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_all_decay(optstmtype optstm, instmtype instm,
														 ::fast_io::uintfpos_t totransmit)
{
	// A true value signature exposes the target aggregate ABI; recursive work
	// borrows the two owner parameters and cannot create another observer copy.
	return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed byte-stream transport. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_bytes_all_decay_dispatch(optstmtype &optstm, instmtype &instm,
								  ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_bytes_all_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/**
 * @brief Transfers an exact byte count with checked temporary-output finish.
 *
 * The finite operation preserves the input position immediately after the
 * requested bytes; it intentionally does not validate or consume logical EOF.
 */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_bytes_all(optstmtype &&optstm, instmtype &&instm,
												   ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_bytes_all_decay_dispatch(output_observer, input_observer, totransmit);
	});
}

} // namespace operations

} // namespace fast_io

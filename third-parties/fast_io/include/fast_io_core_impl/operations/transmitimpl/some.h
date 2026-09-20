#pragma once

/*
 * Bounded partial element transmit operation (IO level).
 *
 * `transmit_some` moves up to the requested equal-width element count and
 * reports progress, using a provider whole-operation CPO or a generic buffered
 * read/write loop. Its completion contract is transfer-specific and should not
 * be confused with primitive `write_some` or scan parse status.
 * The public output guard preserves that result while terminally finishing only
 * an opted-in temporary after successful partial transfer.
 */

namespace fast_io
{

namespace details
{

/** @brief Copies up to a requested element count and reports actual progress. */
template <typename optstmtype, typename instmtype>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr ::fast_io::uintfpos_t transmit_some_main_impl(optstmtype &optstm, instmtype &instm,
															   ::fast_io::uintfpos_t needtransmit)
{
	if (needtransmit == 0u)
	{
		/*
		A zero upper bound admits exactly one result: zero transferred elements.
		Keeping the identity shortcut in the terminal implementation preserves all
		outer normalization, locking, and checked-finish semantics while proving
		that allocation and primitive I/O cannot contribute to the result.
		*/
		return 0u;
	}
	// Generic partial transfer stages through one bounded local element buffer
	// and reports only units that have also reached the output.
	using input_char_type = typename instmtype::input_char_type;
	using output_char_type = typename optstmtype::output_char_type;
	constexpr ::std::size_t default_buffer_elements{
		::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>};
	::std::size_t buffer_elements{default_buffer_elements};
	if (needtransmit < static_cast<::fast_io::uintfpos_t>(default_buffer_elements))
	{
		/*
		The request and capacity are element counts. The character width is used
		only to derive how many elements fit the default byte budget. Comparing in
		uintfpos_t before narrowing proves needtransmit fits size_t in this branch,
		so the allocation is min(default_buffer_elements, remaining_request)
		elements without confusing bytes and elements.
		*/
		buffer_elements = static_cast<::std::size_t>(needtransmit);
	}
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(
		buffer_elements);
	input_char_type *buffer_start{newptr.ptr};
	::fast_io::uintfpos_t totransmit{needtransmit};
	while (totransmit)
	{
		// Attempt one bounded element block and stop on the first short EOF result.
		::std::size_t this_round{buffer_elements};
		if (totransmit < this_round)
		{
			// Bound the current read by the caller's remaining element request.
			this_round = static_cast<::std::size_t>(totransmit);
		}
		input_char_type *iter{
			::fast_io::operations::decay::read_some_decay_dispatch(instm, buffer_start, buffer_start + this_round)};
		if (iter == buffer_start)
		{
			// A zero-length read completes this partial transfer without draining.
			break;
		}
		if constexpr (::std::same_as<output_char_type, input_char_type>)
		{
			// Matching character types use the temporary buffer directly.
			::fast_io::operations::decay::write_all_decay_dispatch(optstm, buffer_start, iter);
		}
		else
		{
			// Equal-width distinct character types use an alias-safe pointer view.
			using output_char_type_may_alias_const_ptrtp
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= output_char_type const *;
			::fast_io::operations::decay::write_all_decay_dispatch(
				optstm, reinterpret_cast<output_char_type_may_alias_const_ptrtp>(buffer_start),
				reinterpret_cast<output_char_type_may_alias_const_ptrtp>(iter));
		}
		totransmit -= static_cast<::std::size_t>(iter - buffer_start);
	}
	return needtransmit - totransmit;
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying partial-transfer mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_some_decay_borrowed(optstmtype &optstm, instmtype &instm,
															 ::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_some_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_some_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Hold the output mutex across the complete partial-transfer operation.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_some_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock input only after output mutex recursion is no longer required.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_some_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Execute the generic partial element loop on unlocked observers.
		return ::fast_io::details::transmit_some_main_impl(optstm, instm, totransmit);
	}
}

/** @brief Owns both observers at the historical partial-transmit value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_some_decay(optstmtype optstm, instmtype instm,
													::fast_io::uintfpos_t totransmit)
{
	// The owner is formed once with the target aggregate ABI; the algorithm then
	// mutates only these named objects through its borrowed recursive graph.
	return ::fast_io::operations::decay::transmit_some_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed transport for each named observer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_some_decay_dispatch(optstmtype &optstm, instmtype &instm,
							 ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_some_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/** @brief Partially transfers elements and checked-finishes an eligible output. */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_some(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_some_decay_dispatch(output_observer, input_observer, totransmit);
	});
}

} // namespace operations

} // namespace fast_io

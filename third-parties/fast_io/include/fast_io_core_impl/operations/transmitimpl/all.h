#pragma once

/*
 * Exact-count element transmit operation (IO level).
 *
 * `transmit_all` moves the requested number of equal-width input/output
 * elements, preferring a provider `status_transmit_all_define` operation and
 * otherwise using the normalized read/write matrix until complete. It treats
 * payload units opaquely and adds no formatting or scanning semantics.
 * The public output is guarded for success-only finish of opted-in temporaries;
 * the input is never implicitly drained beyond the requested element count.
 */

namespace fast_io
{

namespace details
{

/** @brief Copies an exact element count through a bounded temporary buffer. */
template <typename optstmtype, typename instmtype>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr void transmit_all_main_impl(optstmtype &optstm, instmtype &instm,
											 ::fast_io::uintfpos_t totransmit)
{
	if (totransmit == 0u)
	{
		/*
		Zero payload is the identity element of exact transfer.  This terminal
		shortcut deliberately follows observer normalization, mutex recursion,
		and the public output guard: those outer protocols still execute, while
		the zero-count postcondition proves that no staging allocation or primitive
		read/write is observable here.
		*/
		return;
	}
	// Generic transfer intentionally stages through a bounded local buffer;
	// provider-specific zero-copy strategies belong to status transmit CPOs.
	using input_char_type = typename instmtype::input_char_type;
	using output_char_type = typename optstmtype::output_char_type;
	constexpr ::std::size_t default_buffer_elements{
		::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>};
	::std::size_t buffer_elements{default_buffer_elements};
	if (totransmit < static_cast<::fast_io::uintfpos_t>(default_buffer_elements))
	{
		/*
		Both operands denote elements. sizeof(input_char_type) participates only
		in translating the byte-budget policy to an element capacity; it never
		scales the caller's element count. The comparison occurs in uintfpos_t,
		and this branch proves the request is below a size_t constant before the
		narrowing conversion. Consequently the allocation is exactly
		min(default_buffer_elements, remaining_request) elements.
		*/
		buffer_elements = static_cast<::std::size_t>(totransmit);
	}
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(
		buffer_elements);
	input_char_type *buffer_start{newptr.ptr};
	while (totransmit)
	{
		// Move one bounded block while preserving the exact remaining count.
		::std::size_t this_round{buffer_elements};
		if (totransmit < this_round)
		{
			// Limit the final iteration to the exact remaining element count.
			this_round = static_cast<::std::size_t>(totransmit);
		}
		auto iter{buffer_start + this_round};
		::fast_io::operations::decay::read_all_decay_dispatch(instm, buffer_start, iter);
		if constexpr (::std::same_as<output_char_type, input_char_type>)
		{
			// Matching character types can reuse the input buffer directly.
			::fast_io::operations::decay::write_all_decay_dispatch(optstm, buffer_start, iter);
		}
		else
		{
			// Equal-width distinct character types require an alias-safe view.
			using output_char_type_may_alias_const_ptrtp
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= output_char_type const *;
			::fast_io::operations::decay::write_all_decay_dispatch(
				optstm, reinterpret_cast<output_char_type_may_alias_const_ptrtp>(buffer_start),
				reinterpret_cast<output_char_type_may_alias_const_ptrtp>(iter));
		}
		totransmit -= this_round;
	}
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying exact-count mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_all_decay_borrowed(optstmtype &optstm, instmtype &instm,
															::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_all_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_all_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock and unwrap the output for the complete logical transfer.
		// Protocol admission proves that recursive unwrapping preserves the output character domain and changes type;
		// the guard therefore protects the complete logical transmit rather than only one primitive write.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_all_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input only when output needs no mutex recursion.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_all_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Both observers are ready for the generic exact-count transfer loop.
		return ::fast_io::details::transmit_all_main_impl(optstm, instm, totransmit);
	}
}

/**
 * @brief Owns both normalized observers at the historical value-decay boundary.
 * @details The parameters, not forwarding references, preserve the aggregate ABI
 *          selected for an explicit owner call; recursion subsequently borrows them.
 */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_all_decay(optstmtype optstm, instmtype instm,
												   ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_all_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed transport for each named observer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_all_decay_dispatch(optstmtype &optstm, instmtype &instm,
							::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_all_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/**
 * @brief Transfers an exact element count with checked temporary-output finish.
 *
 * Input normalization is lifetime-only and never drains past `totransmit`;
 * output commit occurs only after the entire transfer succeeds.
 */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_all_decay_dispatch(output_observer, input_observer, totransmit);
	});
}

} // namespace operations

} // namespace fast_io

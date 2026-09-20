#pragma once

/*
 * Element transmit-until-EOF operation (IO level).
 *
 * This operation drains an input observer into an equal-width output observer,
 * tracks total progress through the extensible transmit-integer protocol, and
 * prefers provider whole-operation CPOs where available. Generic execution
 * composes normalized read/write primitives under one synchronization plan.
 * Logical EOF completes an input transcoder's terminal drain naturally; after
 * that success the output guard checked-finishes an eligible temporary owner.
 */

namespace fast_io
{

namespace details
{

/** @brief Transfers elements to logical EOF and accumulates generic progress. */
template <typename optstmtype, typename instmtype, typename T>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr void transmit_until_eof_generic_main_impl(optstmtype &optstm, instmtype &instm, T &resultint)
{
	// Generic EOF transfer uses a fixed-size local element buffer while provider
	// status CPOs remain free to supply a zero-copy implementation.
	using input_char_type = typename instmtype::input_char_type;
	using output_char_type = typename optstmtype::output_char_type;
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(
		::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>);
	input_char_type *buffer_start{newptr.ptr};
	input_char_type *buffer_end{newptr.ptr + newptr.size};
	for (input_char_type *iter;
		 (iter = ::fast_io::operations::decay::read_some_decay_dispatch(instm, buffer_start, buffer_end)) != buffer_start;)
	{
		// Forward each nonempty input block and accumulate its element count.
		::std::size_t off{static_cast<::std::size_t>(iter - buffer_start)};
		if constexpr (::std::same_as<output_char_type, input_char_type>)
		{
			// Matching character types write directly from the input buffer.
			::fast_io::operations::decay::write_all_decay_dispatch(optstm, buffer_start, iter);
		}
		else
		{
			// Equal-width distinct types use an alias-safe output view.
			using output_char_type_may_alias_const_ptrtp
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= output_char_type const *;
			::fast_io::operations::decay::write_all_decay_dispatch(
				optstm, reinterpret_cast<output_char_type_may_alias_const_ptrtp>(buffer_start),
				reinterpret_cast<output_char_type_may_alias_const_ptrtp>(iter));
		}
		transmit_integer_add_define(resultint, off);
	}
}

/** @brief Transfers elements to EOF and returns the standard result type. */
template <typename optstmtype, typename instmtype>
inline constexpr ::fast_io::transmit_result transmit_until_eof_main_impl(optstmtype &optstm, instmtype &instm)
{
	::fast_io::uintfpos_t transmitted{};
	uintfpos_transmit_reference_wrapper wrapper{__builtin_addressof(transmitted)};
	::fast_io::details::transmit_until_eof_generic_main_impl(optstm, instm, wrapper);
	return {transmitted};
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows both observers and one accumulator through EOF mutex recursion. */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
inline constexpr decltype(auto) transmit_until_eof_generic_decay_borrowed(optstmtype &optstm, instmtype &instm,
																		  T &resultint)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock output across the complete EOF-driven transfer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(unlocked_output, instm,
																					   resultint);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock input only after output has reached an unlocked observer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(optstm, unlocked_input,
																					   resultint);
	}
	else
	{
		// Run the generic-result EOF loop on unlocked observers.
		return ::fast_io::details::transmit_until_eof_generic_main_impl(optstm, instm, resultint);
	}
}

/**
 * @brief Owns streams and the generic accumulator at the historical value boundary.
 * @details Recursive execution borrows all three named owners, so the progress
 *          adapter denotes one state throughout the complete transfer.
 */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
inline constexpr decltype(auto) transmit_until_eof_generic_decay(optstmtype optstm, instmtype instm, T resultint)
{
	return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(optstm, instm, resultint);
}

/** @brief Selects each stream ABI independently while preserving accumulator identity. */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_until_eof_generic_decay_dispatch(optstmtype &optstm, instmtype &instm,
										  T &resultint)
{
	return ::fast_io::operations::decay::transmit_stream_pair_accumulator_dispatch<
		&::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed<optstmtype, instmtype, T>>(
		optstm, instm, resultint);
}

/** @brief Borrows stable observers through standard-result EOF mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_until_eof_decay_borrowed(optstmtype &optstm, instmtype &instm)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_until_eof_define(optstm,instm);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_until_eof_define(optstm,instm);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Hold the output lock through the complete logical input stream.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(unlocked_output, instm);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Hold the input lock after output mutex recursion is resolved.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(optstm, unlocked_input);
	}
	else
	{
		// Execute standard-result EOF transfer on the unlocked observers.
		return ::fast_io::details::transmit_until_eof_main_impl(optstm, instm);
	}
}

/** @brief Owns both observers at the historical standard-result value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_until_eof_decay(optstmtype optstm, instmtype instm)
{
	return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(optstm, instm);
}

/** @brief Independently selects value or borrowed transport for each named observer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_until_eof_decay_dispatch(optstmtype &optstm, instmtype &instm)
{
	return ::fast_io::operations::decay::transmit_stream_pair_dispatch<
		&::fast_io::operations::decay::transmit_until_eof_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm);
}

} // namespace decay

/** @brief Transfers elements to EOF with a caller-selected progress accumulator. */
template <typename optstmtype, typename instmtype, typename T>
inline constexpr decltype(auto) transmit_until_eof_generic(optstmtype &&optstm, instmtype &&instm, T resultint)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_dispatch(
			output_observer, input_observer, resultint);
	});
}

/** @brief Transfers elements to EOF and checked-finishes an eligible output. */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_until_eof(optstmtype &&optstm, instmtype &&instm)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_until_eof_decay_dispatch(output_observer, input_observer);
	});
}

} // namespace operations

} // namespace fast_io

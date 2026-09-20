#pragma once

/**
 * @file
 * @brief Implements the bidirectional transcoder stream adapter.
 *
 * One underlying handle is shared, but input and output are complete child
 * adapters with independent engines, lifecycle states, source/destination
 * cursors, remainders, and allocations. This separation is mandatory for
 * stateful codecs: decoder and encoder counters or partial blocks are never
 * interchangeable even when their configuration is identical.
 */

namespace fast_io
{

/**
 * @brief Owns independent input and output transcoders over one duplex handle.
 *
 * Directional engines, buffers, cursors, and terminal states are intentionally
 * separate. Only the underlying I/O handle and the input-before-refill tie are
 * shared, preventing decoder state from contaminating encoder state.
 */
template <
	::std::integral input_public_char,
	::std::integral output_public_char,
	typename handle_storage,
	typename input_engine,
	typename output_engine,
	typename input_traits = ::fast_io::basic_itranscoder_traits<
		input_public_char,
		::fast_io::transcode_from_value_t<
			::fast_io::operations::transcode_engine_ref_t<input_engine>>,
		::fast_io::transcode_to_value_t<
			::fast_io::operations::transcode_engine_ref_t<input_engine>>>,
	typename output_traits = ::fast_io::basic_otranscoder_traits<
		output_public_char,
		::fast_io::transcode_to_value_t<
			::fast_io::operations::transcode_engine_ref_t<output_engine>>>>
	requires(::fast_io::transcoder<input_engine> &&
			 ::fast_io::transcoder<output_engine>)
class basic_iotranscoder
{
public:
	using input_char_type = input_public_char;
	using output_char_type = output_public_char;
	using handle_storage_type = handle_storage;
	using input_engine_type = input_engine;
	using output_engine_type = output_engine;
	using input_traits_type = input_traits;
	using output_traits_type = output_traits;
	using shared_handle_type =
		::fast_io::details::iotranscoder_shared_handle<handle_storage_type>;
	using input_adapter_type = ::fast_io::basic_itranscoder<
		input_char_type, shared_handle_type, input_engine_type,
		input_traits_type>;
	using output_adapter_type = ::fast_io::basic_otranscoder<
		output_char_type, shared_handle_type, output_engine_type,
		output_traits_type>;

	handle_storage_type handle;
	input_adapter_type input;
	output_adapter_type output;

	/** @brief Constructs both directional adapters and installs their shared tie. */
	inline explicit basic_iotranscoder(handle_storage_type handle_value,
									   input_engine_type input_engine_value,
									   output_engine_type output_engine_value)
		: handle(::std::move(handle_value)),
		  input(shared_handle_type{__builtin_addressof(handle), this,
								   &flush_tied_output},
				::std::move(input_engine_value)),
		  output(shared_handle_type{__builtin_addressof(handle), nullptr,
									nullptr},
				 ::std::move(output_engine_value))
	{}
	// Member declaration order guarantees that shared storage exists before
	// either child is constructed and outlives both child destructors.

	/** @brief Disables copying because child adapters own mutable protocol state. */
	basic_iotranscoder(basic_iotranscoder const &) = delete;
	/** @brief Disables copy assignment for the same single-owner state invariant. */
	basic_iotranscoder &operator=(basic_iotranscoder const &) = delete;
	/** @brief Disables move assignment to keep child reference topology stable. */
	basic_iotranscoder &operator=(basic_iotranscoder &&) = delete;

	/** @brief Moves complete directional state and rebinds child handle pointers. */
	inline basic_iotranscoder(basic_iotranscoder &&other) noexcept(
		::std::is_nothrow_move_constructible_v<handle_storage_type> &&
		::std::is_nothrow_move_constructible_v<input_adapter_type> &&
		::std::is_nothrow_move_constructible_v<output_adapter_type>)
		: handle(::std::move(other.handle)), input(::std::move(other.input)),
		  output(::std::move(other.output))
	{
		// Child moves preserve engine/buffer state but their shared-handle pointers
		// still name the old parent. Rebind before exposing the new object.
		rebind_children();
	}

	/** @brief Terminally finishes only the output direction. */
	inline void finish_output()
	{
		output.finish();
	}

	/** @brief Discards remaining input and terminally validates its engine. */
	inline void drain_and_finish_input()
	{
		input.drain_and_finish();
	}

private:
	/** @brief Sync-flushes pending output before the input performs physical I/O. */
	inline static void flush_tied_output(void *pointer)
	{
		// Input underflow may make pending prompts/requests visible, but it must
		// never terminally finish the output message. A finished output is already
		// visible and needs no further tie action.
		auto self{static_cast<basic_iotranscoder *>(pointer)};
		if (self->output.state == ::fast_io::otranscoder_state::open)
		{
			// An open encoder may hold bytes that the peer must see before replying.
			self->output.flush();
		}
	}

	/** @brief Retargets moved children to the new parent and shared handle. */
	inline void rebind_children() noexcept
	{
		input.handle.ptr = __builtin_addressof(handle);
		input.handle.tie_ptr = this;
		input.handle.tie_flush = &flush_tied_output;
		output.handle.ptr = __builtin_addressof(handle);
		output.handle.tie_ptr = nullptr;
		output.handle.tie_flush = nullptr;
	}
};

// A temporary duplex owner participates in checked output finish only. Its
// input direction retains the ordinary rule that reads never discard unread
// plaintext merely because the wrapper is an rvalue.
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr bool temporary_output_finish_enabled<
	::fast_io::basic_iotranscoder<
		input_public_char, output_public_char, handle_storage,
		input_engine, output_engine, input_traits, output_traits>> = true;

} // namespace fast_io

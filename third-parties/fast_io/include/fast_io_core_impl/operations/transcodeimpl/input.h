#pragma once

/**
 * @file
 * @brief Implements buffered input over bounded stateful transform engines.
 *
 * Raw source is consumed through the cursor bridge and transformed output is
 * published as an ordinary ibuffer. Exact-unit output can be published
 * directly; std::byte output is assembled into complete PublicChar objects for
 * typed consumers while byte consumers may observe arbitrary prefixes. At
 * physical EOF the adapter drives engine finish so decoders can validate
 * trailers, padding, incomplete encodings, or authentication tags.
 */

namespace fast_io
{

/** @brief Tracks input decoding, terminal validation, and abandonment state. */
enum class itranscoder_state : ::std::uint_least8_t
{
	// More source, output, or terminal validation may remain.
	open,
	// Physical EOF and engine finish both completed with no pending output.
	finished,
	// The owner abandoned unread input without terminal validation.
	cancelled,
	// An engine, allocation, tie flush, or underlying read threw.
	failed
};

/**
 * @brief Adapts a bounded stateful transform engine to a buffered input stream.
 *
 * Physical source, transformed output, and caller-visible objects have distinct
 * cursor ranges. Typed reads preserve complete-object boundaries; byte reads
 * may cross them and record alignment until the byte cursor realigns.
 */
template <::std::integral public_char, typename handle_storage,
		  typename engine,
		  typename traits = ::fast_io::basic_itranscoder_traits<
			  public_char,
			  ::fast_io::transcode_from_value_t<
				  ::fast_io::operations::transcode_engine_ref_t<engine>>,
			  ::fast_io::transcode_to_value_t<
				  ::fast_io::operations::transcode_engine_ref_t<engine>>>>
	requires(::fast_io::transcoder<engine> &&
			 ::fast_io::transcode_automatically_bindable<
				 ::fast_io::transcode_to_value_t<
					 ::fast_io::operations::transcode_engine_ref_t<engine>>,
				 public_char>)
class basic_itranscoder
{
public:
	using input_char_type = public_char;
	using handle_storage_type = handle_storage;
	using engine_type = engine;
	using traits_type = traits;
	using allocator_type = typename traits_type::allocator_type;
	using engine_ref_type =
		::fast_io::operations::transcode_engine_ref_t<engine_type>;
	using source_unit_type =
		::fast_io::transcode_from_value_t<engine_ref_type>;
	using transformed_unit_type =
		::fast_io::transcode_to_value_t<engine_ref_type>;
	using normalized_input_ref_type = ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(
		::std::declval<handle_storage_type &>().get()))>;

	static_assert(::fast_io::transcode_automatically_bindable<
					  source_unit_type,
					  typename normalized_input_ref_type::input_char_type>,
				  "the input stream character type cannot be bound to the transcoder source unit");

	static inline constexpr transcode_unit_binding source_binding{[] {
		if constexpr (::fast_io::transcode_automatically_bindable<
						  source_unit_type,
						  typename normalized_input_ref_type::input_char_type>)
		{
			// Select the valid direct or object-byte binding for the source stream.
			return ::fast_io::transcode_unit_binding_for<
				source_unit_type,
				typename normalized_input_ref_type::input_char_type>();
		}
		else
		{
			// Keep the expression well formed while the preceding assertion diagnoses.
			return transcode_unit_binding::exact_units;
		}
	}()};
	static inline constexpr transcode_unit_binding target_binding{
		::fast_io::transcode_unit_binding_for<transformed_unit_type,
											  input_char_type>()};

	static_assert([]() consteval {
		if constexpr (source_binding == transcode_unit_binding::exact_units)
		{
			// Exact units may arrive from a typed get area or typed read primitive.
			return ::fast_io::operations::decay::defines::
					   has_ibuffer_basic_operations<normalized_input_ref_type> ||
				   ::fast_io::operations::decay::defines::
					   readable<normalized_input_ref_type>;
		}
		else
		{
			// Byte binding accepts byte reads or representation-copyable get areas.
			return ::fast_io::operations::decay::defines::
					   bytes_readable<normalized_input_ref_type> ||
				   ::fast_io::operations::decay::defines::
					   has_ibuffer_basic_operations<normalized_input_ref_type>;
		}
	}());
	static_assert(traits_type::public_buffer_size != 0u);
	static_assert(traits_type::source_buffer_size != 0u);
	static_assert(traits_type::transform_buffer_size != 0u);
	static_assert(
		traits_type::publication_mode ==
			::fast_io::transcode_input_publication_mode::streaming_unverified,
		"hold_until_authenticated requires an authenticated-message storage policy");

	// public_buffer is the caller-visible get area. source_buffer owns raw input
	// when a direct underlying ibuffer cannot be borrowed. transform_buffer is
	// needed only when engine output is std::byte and typed publication must
	// assemble complete PublicChar objects.
	::fast_io::details::basic_transcode_input_buffer_pointers<input_char_type>
		public_buffer{};
	::fast_io::details::basic_transcode_input_buffer_pointers<source_unit_type>
		source_buffer{};
	::fast_io::details::basic_transcode_input_buffer_pointers<
		transformed_unit_type>
		transform_buffer{};
	::fast_io::details::basic_transcode_source_remainder<input_char_type>
		target_remainder{};
	handle_storage_type handle;
	engine_type transcode_engine;
	itranscoder_state state{itranscoder_state::open};
	// Physical EOF and terminal engine completion are separate facts: finish may
	// require multiple destination buffers and completed finish output may still
	// be waiting for the caller in public_buffer/transform_buffer.
	bool source_eof{};
	bool engine_finished{};
	// Offset into the current PublicChar representation after byte-domain reads.
	// Zero is the only state from which typed reads are legal.
	::std::size_t byte_boundary_offset{};

	/** @brief Constructs an open input adapter with lazily allocated buffers. */
	inline explicit basic_itranscoder(handle_storage_type handle_value,
									  engine_type engine_value)
		: handle(::std::move(handle_value)),
		  transcode_engine(::std::move(engine_value))
	{}

	/** @brief Disables copying of decoder, cursor, and allocation ownership. */
	basic_itranscoder(basic_itranscoder const &) = delete;
	/** @brief Disables copy assignment of stateful input ownership. */
	basic_itranscoder &operator=(basic_itranscoder const &) = delete;
	/** @brief Disables move assignment so cursor topology cannot be duplicated. */
	basic_itranscoder &operator=(basic_itranscoder &&) = delete;

	/** @brief Transfers all decoder, buffer, remainder, and lifecycle state. */
	inline basic_itranscoder(basic_itranscoder &&other) noexcept(
		::std::is_nothrow_move_constructible_v<handle_storage_type> &&
		::std::is_nothrow_move_constructible_v<engine_type>)
		: public_buffer(other.public_buffer),
		  source_buffer(other.source_buffer),
		  transform_buffer(other.transform_buffer),
		  target_remainder(other.target_remainder),
		  handle(::std::move(other.handle)),
		  transcode_engine(::std::move(other.transcode_engine)),
		  state(other.state), source_eof(other.source_eof),
		  engine_finished(other.engine_finished),
		  byte_boundary_offset(other.byte_boundary_offset)
	{
		// Transfer allocations and all three cursors as one state snapshot. The
		// moved-from adapter becomes inert and owns no buffers.
		other.public_buffer = {};
		other.source_buffer = {};
		other.transform_buffer = {};
		other.target_remainder.size = 0u;
		if constexpr (traits_type::secure_clear)
		{
			// Erase the moved-from inline remainder before making it inert.
			::fast_io::secure_clear(other.target_remainder.storage,
									sizeof(other.target_remainder.storage));
		}
		other.byte_boundary_offset = 0u;
		other.state = itranscoder_state::cancelled;
	}

	/** @brief Cancels unread input and releases all adapter-owned allocations. */
	inline ~basic_itranscoder()
	{
		// Ordinary input destruction never drains unread plaintext or performs
		// validation with potentially throwing I/O. Call drain_and_finish when a
		// complete message must be authenticated or otherwise verified.
		cancel();
		destroy_buffers();
	}

	/** @brief Requires an active input direction for state-mutating operations. */
	inline void require_open() const
	{
		if (state != itranscoder_state::open) [[unlikely]]
		{
			// Finished, cancelled, and failed engines cannot resume decoding.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::invalid_state);
		}
	}

	/** @brief Requires a valid lifecycle state and a complete object boundary. */
	inline void require_typed_read_boundary() const
	{
		// A typed read after a partial byte read would fabricate an object with a
		// missing prefix. Byte reads may continue until the offset realigns.
		if (state != itranscoder_state::open &&
			state != itranscoder_state::finished) [[unlikely]]
		{
			// Only open and naturally finished adapters permit typed observation.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::invalid_state);
		}
		if (byte_boundary_offset != 0u) [[unlikely]]
		{
			// A typed object cannot begin at a partial byte offset.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::incomplete_unit);
		}
	}

	/** @brief Lazily creates a non-null empty get area for generic read code. */
	inline void prepare_get_area()
	{
		// Generic read/scan code subtracts ibuffer cursors before its first
		// underflow. Lazily allocate an empty but valid range to avoid null-pointer
		// arithmetic while preserving allocation-free adapter construction.
		require_typed_read_boundary();
		if (public_buffer.buffer_begin == nullptr)
		{
			// Allocate only on first cursor exposure to keep construction allocation-free.
			guarded([this] {
				::fast_io::details::ensure_transcode_input_buffer<
					allocator_type, traits_type::secure_clear>(
					public_buffer, 1u, traits_type::public_buffer_size);
			});
		}
	}

	/** @brief Marks the input failed before propagating a stateful operation error. */
	template <typename function>
	inline decltype(auto) guarded(function &&operation)
	{
		// Engine/source/tie progress cannot be rolled back. Failure therefore
		// poisons the direction before the original exception escapes.
#ifdef __cpp_exceptions
		try
		{
			// Execute one complete decoder mutation under the failure boundary.
			return ::std::forward<function>(operation)();
		}
		catch (...)
		{
			// Source or engine progress cannot be safely replayed after failure.
			state = itranscoder_state::failed;
			throw;
		}
#else
		// No-exception builds execute directly and terminate at the failure source.
		return ::std::forward<function>(operation)();
#endif
	}

	/** @brief Invokes an optional duplex output tie before physical input refill. */
	inline void before_source_refill()
	{
		// basic_iotranscoder supplies this hook to implement tied duplex streams.
		// Standalone storage has no such member and compiles to a no-op.
		if constexpr (requires(handle_storage_type &storage) {
						  storage.before_input_refill();
					  })
		{
			// Duplex shared storage exposes the tie; standalone storage does not.
			handle.before_input_refill();
		}
	}

	/** @brief Prepares the next bounded source range through the cursor bridge. */
	template <typename input_ref>
	inline auto prepare_source(input_ref &inref)
	{
		return ::fast_io::details::prepare_transcode_source<
			allocator_type, traits_type::secure_clear>(
			inref, source_buffer, source_eof,
			traits_type::source_buffer_size);
	}

	/** @brief Produces one caller-visible block for an exact-unit engine target. */
	template <typename input_ref>
	inline bool refill_exact(engine_ref_type &engine_ref, input_ref &inref)
	{
		// Exact-unit engines produce directly into the public get area. Return as
		// soon as any output is publishable; otherwise continue through source
		// refills and finally the terminal drain at physical EOF.
		public_buffer.buffer_curr = public_buffer.buffer_end =
			public_buffer.buffer_begin;
		for (;;)
		{
			// Alternate bounded processing and terminal drain until data or EOF.
			if (engine_finished)
			{
				// No engine or source work remains after terminal drain completion.
				state = itranscoder_state::finished;
				return false;
			}
			if (!source_eof)
			{
				// Process physical input until EOF has been observed.
				auto source{prepare_source(inref)};
				if (source.first != source.last)
				{
					// Supply the prepared nonempty source to one bounded process step.
					::std::size_t const minimum{
						::fast_io::operations::decay::
							transcode_min_output_size_decay(
								::fast_io::transcode_reserve<engine_ref_type>,
								::fast_io::transcode_phase::process)};
					::fast_io::details::ensure_transcode_input_buffer<
						allocator_type, traits_type::secure_clear>(
						public_buffer, minimum,
						traits_type::public_buffer_size);
					auto result{
						::fast_io::operations::decay::transcode_process_decay_dispatch(
							engine_ref, source.first, source.last,
							public_buffer.buffer_begin,
							public_buffer.buffer_capacity_end)};
					if (result.status !=
							::fast_io::transcode_step_status::need_input &&
						result.status !=
							::fast_io::transcode_step_status::need_output)
					{
						// Only protocol-defined process statuses may be returned.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					auto const validated_to{
						::fast_io::details::validate_transcode_closed_range_offsets(
							public_buffer.buffer_begin,
							public_buffer.buffer_capacity_end, result.to_next)};
					if (!validated_to.valid) [[unlikely]]
					{
						// Prove the provider cursor's integer offset before any progress
						// comparison or installation into adapter-owned state.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					auto const validated_from{
						::fast_io::details::commit_transcode_source(
							inref, source_buffer, source, result.from_next)};
					bool const made_progress{
						validated_from != source.first ||
						validated_to.current_offset != 0u};
					public_buffer.buffer_curr = public_buffer.buffer_begin;
					public_buffer.buffer_end = validated_to.current;
					if (result.status ==
							::fast_io::transcode_step_status::need_input &&
						validated_from != source.last)
					{
						// Need-input must account for the complete prepared source range.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					if (result.status ==
							::fast_io::transcode_step_status::need_output &&
						!made_progress)
					{
						// Need-output without progress would livelock on fresh capacity.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					if (public_buffer.buffer_curr != public_buffer.buffer_end)
					{
						// Publish produced units immediately for streaming latency.
						return true;
					}
					continue;
				}
			}

			::std::size_t const minimum{
				::fast_io::operations::decay::
					transcode_min_output_size_decay(
						::fast_io::transcode_reserve<engine_ref_type>,
						::fast_io::transcode_phase::finish)};
			::fast_io::details::ensure_transcode_input_buffer<
				allocator_type, traits_type::secure_clear>(
				public_buffer, minimum, traits_type::public_buffer_size);
			auto result{
				::fast_io::operations::decay::transcode_finish_decay_dispatch(
					engine_ref, public_buffer.buffer_begin,
					public_buffer.buffer_capacity_end)};
			if (result.status != ::fast_io::transcode_drain_status::complete &&
				result.status !=
					::fast_io::transcode_drain_status::need_output)
			{
				// Terminal drain accepts only complete or need-output status.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			auto const validated_to{
				::fast_io::details::validate_transcode_closed_range_offsets(
					public_buffer.buffer_begin,
					public_buffer.buffer_capacity_end, result.to_next)};
			if (!validated_to.valid) [[unlikely]]
			{
				// Terminal output crosses the same untrusted engine boundary and
				// therefore obeys the identical closed-range offset proof.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			bool const made_progress{validated_to.current_offset != 0u};
			public_buffer.buffer_curr = public_buffer.buffer_begin;
			public_buffer.buffer_end = validated_to.current;
			if (result.status == ::fast_io::transcode_drain_status::complete)
			{
				// Record terminal engine completion even if output still awaits reading.
				engine_finished = true;
			}
			else if (!made_progress)
			{
				// Need-output must produce something into guaranteed capacity.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			if (made_progress)
			{
				// Publish terminally generated output before reporting logical EOF.
				return true;
			}
			state = itranscoder_state::finished;
			return false;
		}
	}

	/** @brief Produces one raw transformed block for an object-byte target. */
	template <typename input_ref>
	inline bool refill_transformed(engine_ref_type &engine_ref,
								   input_ref &inref)
	{
		// This is the raw-byte counterpart of refill_exact. It intentionally does
		// not impose PublicChar alignment, because byte reads and explicit drain
		// must be able to consume a terminal partial object representation.
		transform_buffer.buffer_curr = transform_buffer.buffer_end =
			transform_buffer.buffer_begin;
		for (;;)
		{
			// Alternate bounded processing and terminal drain until raw output or EOF.
			if (engine_finished)
			{
				// No raw output remains after the terminal drain has completed.
				state = itranscoder_state::finished;
				return false;
			}
			if (!source_eof)
			{
				// Continue bounded processing while physical source may remain.
				auto source{prepare_source(inref)};
				if (source.first != source.last)
				{
					// Process a prepared nonempty source into raw transformed scratch.
					::std::size_t const minimum{
						::fast_io::operations::decay::
							transcode_min_output_size_decay(
								::fast_io::transcode_reserve<engine_ref_type>,
								::fast_io::transcode_phase::process)};
					::fast_io::details::ensure_transcode_input_buffer<
						allocator_type, traits_type::secure_clear>(
						transform_buffer, minimum,
						traits_type::transform_buffer_size);
					auto result{
						::fast_io::operations::decay::transcode_process_decay_dispatch(
							engine_ref, source.first, source.last,
							transform_buffer.buffer_begin,
							transform_buffer.buffer_capacity_end)};
					if (result.status !=
							::fast_io::transcode_step_status::need_input &&
						result.status !=
							::fast_io::transcode_step_status::need_output)
					{
						// Reject statuses outside the two-state process protocol.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					auto const validated_to{
						::fast_io::details::validate_transcode_closed_range_offsets(
							transform_buffer.buffer_begin,
							transform_buffer.buffer_capacity_end, result.to_next)};
					if (!validated_to.valid) [[unlikely]]
					{
						// Validate the raw destination cursor without evaluating an
						// unrelated-pointer comparison supplied by the engine.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					auto const validated_from{
						::fast_io::details::commit_transcode_source(
							inref, source_buffer, source, result.from_next)};
					bool const made_progress{
						validated_from != source.first ||
						validated_to.current_offset != 0u};
					transform_buffer.buffer_curr =
						transform_buffer.buffer_begin;
					transform_buffer.buffer_end = validated_to.current;
					if (result.status ==
							::fast_io::transcode_step_status::need_input &&
						validated_from != source.last)
					{
						// Need-input is valid only after consuming the supplied source.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					if (result.status ==
							::fast_io::transcode_step_status::need_output &&
						!made_progress)
					{
						// Need-output must make progress with guaranteed destination space.
						::fast_io::throw_transcode_stream_error(
							::fast_io::transcode_stream_errc::protocol_violation);
					}
					if (transform_buffer.buffer_curr !=
						transform_buffer.buffer_end)
					{
						// Expose raw output to byte copying or typed-object assembly.
						return true;
					}
					continue;
				}
			}

			::std::size_t const minimum{
				::fast_io::operations::decay::
					transcode_min_output_size_decay(
						::fast_io::transcode_reserve<engine_ref_type>,
						::fast_io::transcode_phase::finish)};
			::fast_io::details::ensure_transcode_input_buffer<
				allocator_type, traits_type::secure_clear>(
				transform_buffer, minimum,
				traits_type::transform_buffer_size);
			auto result{
				::fast_io::operations::decay::transcode_finish_decay_dispatch(
					engine_ref, transform_buffer.buffer_begin,
					transform_buffer.buffer_capacity_end)};
			if (result.status != ::fast_io::transcode_drain_status::complete &&
				result.status !=
					::fast_io::transcode_drain_status::need_output)
			{
				// Terminal drain accepts only complete or need-output status.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			auto const validated_to{
				::fast_io::details::validate_transcode_closed_range_offsets(
					transform_buffer.buffer_begin,
					transform_buffer.buffer_capacity_end, result.to_next)};
			if (!validated_to.valid) [[unlikely]]
			{
				// Reject terminal cursors before they become persistent adapter
				// state; successful cursors are rebuilt from the owned base.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			bool const made_progress{validated_to.current_offset != 0u};
			transform_buffer.buffer_curr = transform_buffer.buffer_begin;
			transform_buffer.buffer_end = validated_to.current;
			if (result.status == ::fast_io::transcode_drain_status::complete)
			{
				// Remember finish completion while preserving any produced raw bytes.
				engine_finished = true;
			}
			else if (!made_progress)
			{
				// Need-output without output would repeat forever on fresh scratch.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			if (made_progress)
			{
				// Return terminal output before marking the public stream exhausted.
				return true;
			}
			state = itranscoder_state::finished;
			return false;
		}
	}

	/** @brief Assembles transformed bytes into complete public input objects. */
	template <typename input_ref>
	inline bool refill_typed_from_bytes(engine_ref_type &engine_ref,
										input_ref &inref)
	{
		// Assemble only complete PublicChar objects with memcpy. If at least one
		// object is ready, publish it before requesting more raw output; this
		// prevents a later malformed suffix from withholding an earlier valid
		// streaming prefix. A final partial object fails on the next underflow.
		::fast_io::details::ensure_transcode_input_buffer<
			allocator_type, traits_type::secure_clear>(
			public_buffer, 1u, traits_type::public_buffer_size);
		public_buffer.buffer_curr = public_buffer.buffer_begin;
		public_buffer.buffer_end = public_buffer.buffer_begin;
		auto destination{public_buffer.buffer_begin};
		for (;;)
		{
			// Assemble complete objects until the public get area is full or EOF.
			if (destination == public_buffer.buffer_capacity_end)
			{
				// Stop once the next caller-visible get area is full.
				break;
			}
			if (target_remainder.size != 0u)
			{
				// Complete an earlier partial public-object representation first.
				while (target_remainder.size != sizeof(input_char_type))
				{
					// Pull raw bytes until the retained public object is complete.
					if (transform_buffer.buffer_curr ==
						transform_buffer.buffer_end)
					{
						// Refill raw transformed bytes only after consuming the current block.
						if (destination != public_buffer.buffer_begin)
						{
							// Publish completed objects before performing another engine step.
							public_buffer.buffer_end = destination;
							return true;
						}
						if (!refill_transformed(engine_ref, inref))
						{
							// Logical EOF with a partial object is a typed-input error.
							::fast_io::throw_transcode_stream_error(
								::fast_io::transcode_stream_errc::incomplete_unit);
						}
					}
					target_remainder.storage[target_remainder.size++] =
						*transform_buffer.buffer_curr++;
				}
				::fast_io::freestanding::my_memcpy(
					destination, target_remainder.storage,
					sizeof(input_char_type));
				++destination;
				if constexpr (traits_type::secure_clear)
				{
					// Erase inline assembly bytes after publishing the complete object.
					::fast_io::secure_clear(target_remainder.storage,
											sizeof(target_remainder.storage));
				}
				target_remainder.size = 0u;
				continue;
			}
			if (transform_buffer.buffer_curr == transform_buffer.buffer_end)
			{
				// Acquire another transformed block after consuming the current one.
				if (!refill_transformed(engine_ref, inref))
				{
					// Terminal exhaustion ends assembly when no partial object exists.
					break;
				}
			}
			::std::size_t const raw_bytes{static_cast<::std::size_t>(
				transform_buffer.buffer_end - transform_buffer.buffer_curr)};
			::std::size_t const available_units{static_cast<::std::size_t>(
				public_buffer.buffer_capacity_end - destination)};
			::std::size_t raw_units{raw_bytes / sizeof(input_char_type)};
			if (raw_units > available_units)
			{
				// Limit bulk assembly to the remaining public-buffer capacity.
				raw_units = available_units;
			}
			if (raw_units != 0u)
			{
				// Copy the maximal complete-object prefix with representation semantics.
				::std::size_t const bytes{raw_units * sizeof(input_char_type)};
				::fast_io::freestanding::my_memcpy(
					destination, transform_buffer.buffer_curr, bytes);
				destination += raw_units;
				transform_buffer.buffer_curr += bytes;
				continue;
			}
			while (transform_buffer.buffer_curr !=
				   transform_buffer.buffer_end)
			{
				// Retain a raw suffix too small to form one complete public object.
				target_remainder.storage[target_remainder.size++] =
					*transform_buffer.buffer_curr++;
			}
		}
		public_buffer.buffer_end = destination;
		return public_buffer.buffer_curr != public_buffer.buffer_end;
	}

	/** @brief Refills the public get area and performs the duplex tie exactly once. */
	template <typename input_ref>
	inline bool underflow_impl(engine_ref_type &engine_ref, input_ref &inref)
	{
		// Duplex tie semantics are attached to logical input underflow, not every
		// low-level source read performed while producing one public buffer.
		if (public_buffer.buffer_curr != public_buffer.buffer_end)
		{
			// Preserve already published caller-visible data.
			return true;
		}
		if (state == itranscoder_state::finished)
		{
			// A naturally finished adapter has no further public data.
			return false;
		}
		before_source_refill();
		if constexpr (target_binding == transcode_unit_binding::exact_units)
		{
			// Exact transformed units publish directly as public input objects.
			return refill_exact(engine_ref, inref);
		}
		else
		{
			// Byte output must be assembled into complete public input objects.
			return refill_typed_from_bytes(engine_ref, inref);
		}
	}

	/** @brief Implements buffered underflow for typed read and scan operations. */
	inline bool underflow()
	{
		require_typed_read_boundary();
		if (public_buffer.buffer_curr != public_buffer.buffer_end)
		{
			// Return immediately when the current get area still contains data.
			return true;
		}
		if (state == itranscoder_state::finished)
		{
			// Report stable EOF after successful terminal validation.
			return false;
		}
		return guarded([this] {
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) inref{
				::fast_io::operations::input_stream_ref(handle.get())};
			return underflow_impl(engine_ref, inref);
		});
	}

	/** @brief Copies at most one currently available block to a typed destination. */
	inline input_char_type *read_some(input_char_type *first,
									  input_char_type *last)
	{
		// Match ordinary read_some semantics: publish at most the currently
		// available transformed block and let read_all own any retry loop.
		require_typed_read_boundary();
		if (first == last || state == itranscoder_state::finished)
		{
			// Empty requests and stable EOF complete without touching the engine.
			return first;
		}
		return guarded([this, first, last] {
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) inref{
				::fast_io::operations::input_stream_ref(handle.get())};
			if (public_buffer.buffer_curr == public_buffer.buffer_end &&
				!underflow_impl(engine_ref, inref))
			{
				// A failed refill is validated logical EOF for this read_some call.
				return first;
			}
			::std::size_t available{static_cast<::std::size_t>(
				public_buffer.buffer_end - public_buffer.buffer_curr)};
			::std::size_t requested{
				static_cast<::std::size_t>(last - first)};
			if (available > requested)
			{
				// Respect read_some's caller-provided destination bound.
				available = requested;
			}
			::fast_io::freestanding::my_memcpy(
				first, public_buffer.buffer_curr,
				available * sizeof(input_char_type));
			public_buffer.buffer_curr += available;
			return first + available;
		});
	}

	/** @brief Advances and wraps the byte cursor within a public input object. */
	inline void advance_byte_boundary(::std::size_t count) noexcept
	{
		byte_boundary_offset =
			(byte_boundary_offset + count) % sizeof(input_char_type);
	}

	/** @brief Copies bytes from already published public objects in stream order. */
	inline ::std::byte *copy_public_bytes(::std::byte *first,
										  ::std::byte *last) noexcept
	{
		// buffer_curr denotes the containing PublicChar while
		// byte_boundary_offset denotes the first unconsumed byte within it.
		if (first == last || public_buffer.buffer_curr == public_buffer.buffer_end)
		{
			// Nothing is copied for an empty destination or exhausted public range.
			return first;
		}
		auto source{reinterpret_cast<::std::byte const *>(
						public_buffer.buffer_curr) +
					byte_boundary_offset};
		::std::size_t available{
			static_cast<::std::size_t>(public_buffer.buffer_end -
									   public_buffer.buffer_curr) *
				sizeof(input_char_type) -
			byte_boundary_offset};
		::std::size_t requested{static_cast<::std::size_t>(last - first)};
		if (available > requested)
		{
			// Limit the copy to the caller's remaining byte destination.
			available = requested;
		}
		::fast_io::freestanding::my_memcpy(first, source, available);
		::std::size_t const total{byte_boundary_offset + available};
		public_buffer.buffer_curr += total / sizeof(input_char_type);
		byte_boundary_offset = total % sizeof(input_char_type);
		return first + available;
	}

	/** @brief Copies and removes bytes retained in the typed-assembly remainder. */
	inline ::std::byte *copy_remainder_bytes(::std::byte *first,
											 ::std::byte *last) noexcept
	{
		// target_remainder is a FIFO prefix of the logical byte stream. Shift its
		// small fixed-capacity storage after a partial byte read.
		::std::size_t count{target_remainder.size};
		::std::size_t requested{static_cast<::std::size_t>(last - first)};
		if (count > requested)
		{
			// Leave any suffix queued when the destination cannot accept it all.
			count = requested;
		}
		if (count == 0u)
		{
			// Avoid memcpy and cursor updates when the remainder is empty.
			return first;
		}
		::fast_io::freestanding::my_memcpy(
			first, target_remainder.storage, count);
		for (::std::size_t index{}; index + count < target_remainder.size;
			 ++index)
		{
			// Compact the unread remainder suffix to the beginning of inline storage.
			target_remainder.storage[index] =
				target_remainder.storage[index + count];
		}
		target_remainder.size -= count;
		advance_byte_boundary(count);
		return first + count;
	}

	/** @brief Copies bytes directly from the current transformed scratch block. */
	inline ::std::byte *copy_transformed_bytes(::std::byte *first,
											   ::std::byte *last) noexcept
	{
		::std::size_t count{static_cast<::std::size_t>(
			transform_buffer.buffer_end - transform_buffer.buffer_curr)};
		::std::size_t requested{static_cast<::std::size_t>(last - first)};
		if (count > requested)
		{
			// Preserve the unrequested transformed suffix for a later byte read.
			count = requested;
		}
		if (count == 0u)
		{
			// An exhausted transformed block contributes no bytes.
			return first;
		}
		::fast_io::freestanding::my_memcpy(
			first, transform_buffer.buffer_curr, count);
		transform_buffer.buffer_curr += count;
		advance_byte_boundary(count);
		return first + count;
	}

	/** @brief Reads one byte-domain block without requiring object alignment. */
	inline ::std::byte *read_some_bytes(::std::byte *first,
										::std::byte *last)
	{
		// Preserve stream order across three possible stores: already published
		// PublicChar objects, a typed-assembly remainder, and raw transformed
		// bytes. Only refill once all earlier stores are exhausted.
		if (state != itranscoder_state::open &&
			state != itranscoder_state::finished) [[unlikely]]
		{
			// Cancelled or failed input cannot expose further transformed bytes.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::invalid_state);
		}
		if (first == last || state == itranscoder_state::finished)
		{
			// Empty requests and validated EOF return without additional work.
			return first;
		}
		return guarded([this, first, last] {
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) inref{
				::fast_io::operations::input_stream_ref(handle.get())};
			auto current{first};
			bool tied_flushed{};
			for (;;)
			{
				// Drain queued stores in logical order, then perform at most needed refill.
				current = copy_public_bytes(current, last);
				if (current == last)
				{
					// Satisfy the request entirely from already published public data.
					return current;
				}
				if constexpr (target_binding ==
							  transcode_unit_binding::exact_units)
				{
					// Exact targets refill public units and expose their representations.
					if (!tied_flushed)
					{
						// Fire the duplex tie once before this logical read performs I/O.
						before_source_refill();
						tied_flushed = true;
					}
					if (!refill_exact(engine_ref, inref))
					{
						// Return the prefix already copied when validated EOF is reached.
						return current;
					}
				}
				else
				{
					// Byte targets drain assembly remainder and raw scratch in order.
					current = copy_remainder_bytes(current, last);
					current = copy_transformed_bytes(current, last);
					if (current == last)
					{
						// Existing queued bytes fully satisfy the request.
						return current;
					}
					if (!tied_flushed)
					{
						// Fire the duplex tie once before requesting more physical input.
						before_source_refill();
						tied_flushed = true;
					}
					if (!refill_transformed(engine_ref, inref))
					{
						// Return a short byte read after validated logical EOF.
						return current;
					}
				}
			}
		});
	}

	/** @brief Explicitly discards unread output and drives input validation to EOF. */
	inline void drain_and_finish()
	{
		// Explicit message validation discards all caller-visible plaintext, reads
		// physical EOF, and drives terminal finish. The raw path deliberately
		// ignores typed alignment because discarded bytes need not form objects.
		if (state == itranscoder_state::finished)
		{
			// A successful prior drain or natural EOF is idempotently complete.
			return;
		}
		require_open();
		guarded([this] {
			public_buffer.buffer_curr = public_buffer.buffer_end;
			target_remainder.size = 0u;
			transform_buffer.buffer_curr = transform_buffer.buffer_end;
			byte_boundary_offset = 0u;
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) inref{
				::fast_io::operations::input_stream_ref(handle.get())};
			before_source_refill();
			if constexpr (target_binding ==
						  transcode_unit_binding::exact_units)
			{
				// Discard each exact-unit public block until terminal completion.
				while (refill_exact(engine_ref, inref))
				{
					// Discard each published exact-unit block during explicit validation.
					public_buffer.buffer_curr = public_buffer.buffer_end;
				}
			}
			else
			{
				// Discard raw transformed bytes without imposing typed alignment.
				while (refill_transformed(engine_ref, inref))
				{
					// Discard each raw block during alignment-agnostic validation.
					transform_buffer.buffer_curr =
						transform_buffer.buffer_end;
				}
			}
			state = itranscoder_state::finished;
		});
	}

	/** @brief Abandons unread input without further I/O or terminal validation. */
	inline void cancel() noexcept
	{
		// Cancellation is a nonthrowing abandonment path. It never reads more
		// source and never claims that terminal validation succeeded.
		if (state == itranscoder_state::open)
		{
			// Preserve finished/failed diagnoses; only active input becomes cancelled.
			state = itranscoder_state::cancelled;
		}
		public_buffer.buffer_curr = public_buffer.buffer_end =
			public_buffer.buffer_begin;
		source_buffer.buffer_curr = source_buffer.buffer_end =
			source_buffer.buffer_begin;
		transform_buffer.buffer_curr = transform_buffer.buffer_end =
			transform_buffer.buffer_begin;
		if constexpr (traits_type::secure_clear)
		{
			// Erase inline partial-object bytes that will never be published.
			::fast_io::secure_clear(target_remainder.storage,
									sizeof(target_remainder.storage));
		}
		target_remainder.size = 0u;
		byte_boundary_offset = 0u;
	}

private:
	/** @brief Securely clears and deallocates one optional input buffer. */
	template <typename unit_type>
	inline static void destroy_buffer(
		::fast_io::details::basic_transcode_input_buffer_pointers<unit_type>
			&buffer) noexcept
	{
		unit_type *begin{buffer.buffer_begin};
		if (begin == nullptr)
		{
			// A buffer never allocated by the lazy policy needs no cleanup.
			return;
		}
		::std::size_t const count{static_cast<::std::size_t>(
			buffer.buffer_capacity_end - begin)};
		if constexpr (traits_type::secure_clear)
		{
			// Apply the traits clearing policy before allocator release.
			::fast_io::secure_clear(begin, count * sizeof(unit_type));
		}
		::fast_io::typed_generic_allocator_adapter<
			allocator_type, unit_type>::deallocate_n(begin, count);
		buffer = {};
	}

	/** @brief Releases public, source, and transformed input allocations. */
	inline void destroy_buffers() noexcept
	{
		destroy_buffer(public_buffer);
		destroy_buffer(source_buffer);
		destroy_buffer(transform_buffer);
	}
};

} // namespace fast_io

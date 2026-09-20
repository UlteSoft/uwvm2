#pragma once

/**
 * @file
 * @brief Bridges bounded transform-engine cursors to ordinary streams.
 *
 * An engine sees only contiguous [first,last) ranges. Exact endpoint types may
 * borrow an underlying stream buffer directly; every other route uses adapter-
 * owned scratch storage and the normal typed/byte read or write primitives.
 * prepare_* never commits speculative engine progress. commit_* validates the
 * returned cursor and advances exactly the range accepted or produced.
 */

namespace fast_io::details
{

/** @brief Tracks begin/current/end cursors for reusable output scratch storage. */
template <typename T>
struct basic_transcode_buffer_pointers
{
	// Output scratch uses buffer_end as the allocation end; no live prefix is
	// retained after commit.
	T *buffer_begin{};
	T *buffer_curr{};
	T *buffer_end{};
};

/** @brief Tracks live and allocated ranges for persistent input scratch storage. */
template <typename T>
struct basic_transcode_input_buffer_pointers
{
	// Input storage separates the live range end from allocation capacity so
	// unconsumed source and published get-area data survive across calls.
	T *buffer_begin{};
	T *buffer_curr{};
	T *buffer_end{};
	T *buffer_capacity_end{};
};

/** @brief Retains an incomplete object representation across byte-bound calls. */
template <::std::integral public_char>
struct basic_transcode_source_remainder
{
	// Object-byte binding may stop between bytes of one PublicChar. The bytes
	// remain opaque and are assembled/disassembled only through memcpy.
	::std::byte storage[sizeof(public_char)]{};
	::std::size_t size{};
};

/** @brief Describes a prepared writable engine destination and its ownership. */
template <::fast_io::transcode_unit unit_type>
struct basic_transcode_destination
{
	unit_type *first{};
	unit_type *last{};
	bool direct{};
};

/** @brief Describes a prepared readable engine source and its ownership. */
template <::fast_io::transcode_unit unit_type>
struct basic_transcode_source
{
	unit_type const *first{};
	unit_type const *last{};
	bool direct{};
};

/** @brief Acquires and validates one coherent underlying input-buffer snapshot. */
template <typename input_ref>
[[nodiscard]] inline auto acquire_transcode_ibuffer_range(input_ref &inref)
{
	auto begin{ibuffer_begin(inref)};
	auto current{ibuffer_curr(inref)};
	auto end{ibuffer_end(inref)};
	auto validated{::fast_io::details::validate_transcode_closed_range_offsets(
		begin, end, current)};
	if (!validated.valid) [[unlikely]]
	{
		// Stream CPO results cross an untrusted customization boundary. Reject a
		// malformed end/current topology before any pointer relation or difference.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	return validated;
}

/** @brief Acquires and validates one coherent underlying output-buffer snapshot. */
template <typename output_ref>
[[nodiscard]] inline auto acquire_transcode_obuffer_range(output_ref &outref)
{
	auto begin{obuffer_begin(outref)};
	auto current{obuffer_curr(outref)};
	auto end{obuffer_end(outref)};
	auto validated{::fast_io::details::validate_transcode_closed_range_offsets(
		begin, end, current)};
	if (!validated.valid) [[unlikely]]
	{
		// Only integer-address validation may inspect provider cursors before the
		// returned current cursor is rebuilt from the reported range base.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	return validated;
}

/** @brief Ensures reusable output scratch meets the engine's minimum capacity. */
template <typename allocator_type, bool secure_clear,
		  ::fast_io::transcode_unit unit_type>
inline void ensure_transcode_scratch(
	::fast_io::details::basic_transcode_buffer_pointers<unit_type> &scratch,
	::std::size_t minimum, ::std::size_t preferred)
{
	// Output scratch is resized only between engine calls, when no produced
	// prefix remains pending.
	if (minimum == 0u)
	{
		// Every engine call receives at least one writable destination unit.
		minimum = 1u;
	}
	if (scratch.buffer_begin != nullptr &&
		static_cast<::std::size_t>(scratch.buffer_end - scratch.buffer_begin) >= minimum)
	{
		// Retain an existing allocation that already satisfies the contract.
		return;
	}
	::std::size_t capacity{preferred < minimum ? minimum : preferred};
	if (capacity == 0u)
	{
		// Defend against a zero preferred size after minimum normalization.
		capacity = 1u;
	}
	using typed_allocator =
		::fast_io::typed_generic_allocator_adapter<allocator_type, unit_type>;
	unit_type *replacement{typed_allocator::allocate(capacity)};
	unit_type *old_begin{scratch.buffer_begin};
	::std::size_t old_capacity{};
	if (old_begin != nullptr)
	{
		// Measure the old allocation before replacing its cursor record.
		old_capacity = static_cast<::std::size_t>(scratch.buffer_end - old_begin);
	}
	scratch = {replacement, replacement, replacement + capacity};
	if (old_begin != nullptr)
	{
		// Dispose of the superseded allocation only after replacement succeeds.
		if constexpr (secure_clear)
		{
			// Erase potentially sensitive transformed output before deallocation.
			::fast_io::secure_clear(old_begin,
									old_capacity * sizeof(unit_type));
		}
		typed_allocator::deallocate_n(old_begin, old_capacity);
	}
}

/** @brief Ensures exhausted input storage has enough capacity for a refill. */
template <typename allocator_type, bool secure_clear, typename unit_type>
inline void ensure_transcode_input_buffer(
	::fast_io::details::basic_transcode_input_buffer_pointers<unit_type> &buffer,
	::std::size_t minimum, ::std::size_t preferred)
{
	// Callers request growth only for an exhausted logical range. Replacing the
	// allocation therefore cannot discard live input or published output.
	if (minimum == 0u)
	{
		// A refill buffer must hold at least one complete source unit.
		minimum = 1u;
	}
	if (buffer.buffer_begin != nullptr &&
		static_cast<::std::size_t>(buffer.buffer_capacity_end -
								   buffer.buffer_begin) >= minimum)
	{
		// Reuse adequate storage; callers guarantee that its live range is empty.
		return;
	}
	::std::size_t capacity{preferred < minimum ? minimum : preferred};
	if (capacity == 0u)
	{
		// Defend against a zero preferred size after minimum normalization.
		capacity = 1u;
	}
	using typed_allocator =
		::fast_io::typed_generic_allocator_adapter<allocator_type, unit_type>;
	unit_type *replacement{typed_allocator::allocate(capacity)};
	unit_type *old_begin{buffer.buffer_begin};
	::std::size_t old_capacity{};
	if (old_begin != nullptr)
	{
		// Capture the old capacity before installing replacement cursors.
		old_capacity = static_cast<::std::size_t>(
			buffer.buffer_capacity_end - old_begin);
	}
	buffer = {replacement, replacement, replacement,
			  replacement + capacity};
	if (old_begin != nullptr)
	{
		// Release old exhausted storage after successful allocation.
		if constexpr (secure_clear)
		{
			// Erase potentially sensitive source material before deallocation.
			::fast_io::secure_clear(old_begin,
									old_capacity * sizeof(unit_type));
		}
		typed_allocator::deallocate_n(old_begin, old_capacity);
	}
}

/**
 * @brief Prepares a bounded engine source without committing stream progress.
 *
 * A direct exact-unit ibuffer is borrowed until commit. All other paths copy
 * into adapter-owned storage so an unconsumed suffix survives `need_output`.
 */
template <typename allocator_type, bool secure_clear,
		  ::fast_io::transcode_unit unit_type, typename input_ref>
inline basic_transcode_source<unit_type> prepare_transcode_source(
	input_ref &inref,
	::fast_io::details::basic_transcode_input_buffer_pointers<unit_type> &scratch,
	bool &source_eof, ::std::size_t preferred)
{
	// An engine may have returned need_output before consuming all scratch
	// input. That suffix always precedes any newly read underlying data.
	if (scratch.buffer_curr != scratch.buffer_end)
	{
		// Resume the unconsumed scratch suffix before reading new physical input.
		return {scratch.buffer_curr, scratch.buffer_end, false};
	}
	if (source_eof)
	{
		// Once physical EOF is observed, subsequent preparations remain empty.
		return {};
	}
	if constexpr (::std::same_as<unit_type,
								 typename input_ref::input_char_type> &&
				  ::fast_io::operations::decay::defines::
					  has_ibuffer_basic_operations<input_ref>)
	{
		// Borrow exact typed get-area storage for the zero-copy input path.
		// Exact typed ibuffers are the zero-copy path. Cursor ownership remains
		// with the underlying stream until commit_transcode_source succeeds.
		auto range{::fast_io::details::acquire_transcode_ibuffer_range(inref)};
		if (range.current_offset == range.end_offset)
		{
			// Refill the underlying get area only when its current range is empty.
			if (!ibuffer_underflow(inref))
			{
				// A false underflow result is the physical EOF observation.
				source_eof = true;
				return {};
			}
			range = ::fast_io::details::acquire_transcode_ibuffer_range(inref);
			if (range.current_offset == range.end_offset)
			{
				// A successful underflow must publish at least one source unit.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
		}
		return {range.current, range.end, true};
	}
	else
	{
		// Non-direct sources are materialized in persistent adapter scratch.
		::fast_io::details::ensure_transcode_input_buffer<
			allocator_type, secure_clear>(
			scratch,
			[]() consteval {
				if constexpr (::std::same_as<unit_type, ::std::byte> &&
							  ::fast_io::operations::decay::defines::
								  has_ibuffer_basic_operations<input_ref>)
				{
					// Byte views of typed ibuffers must hold one complete object.
					return sizeof(typename input_ref::input_char_type);
				}
				else
				{
					// Scalar typed or native byte reads need only one engine unit.
					return 1u;
				}
			}(),
			preferred);
		unit_type *next{};
		if constexpr (::std::same_as<
						  unit_type, typename input_ref::input_char_type>)
		{
			// Matching endpoint types use the normal typed read primitive.
			next = ::fast_io::operations::decay::read_some_decay_dispatch(
				inref, scratch.buffer_begin, scratch.buffer_capacity_end);
		}
		else if constexpr (::std::same_as<unit_type, ::std::byte> &&
						   ::fast_io::operations::decay::defines::
							   has_ibuffer_basic_operations<input_ref>)
		{
			// Bridge a typed-only get area through complete object bytes.
			// A pure ibuffer need not advertise a separate byte-read primitive.
			// Copy complete underlying objects into byte scratch and commit their
			// typed cursor immediately; scratch then owns those bytes until the
			// engine consumes them, including across need_output returns.
			auto range{::fast_io::details::acquire_transcode_ibuffer_range(inref)};
			if (range.current_offset == range.end_offset)
			{
				// Ask the underlying buffered input to publish another typed range.
				if (!ibuffer_underflow(inref))
				{
					// Record EOF and leave scratch in its canonical empty state.
					source_eof = true;
					scratch.buffer_curr = scratch.buffer_end =
						scratch.buffer_begin;
					return {};
				}
				range = ::fast_io::details::acquire_transcode_ibuffer_range(inref);
				if (range.current_offset == range.end_offset)
				{
					// Successful underflow with no data violates the stream protocol.
					::fast_io::throw_transcode_stream_error(
						::fast_io::transcode_stream_errc::protocol_violation);
				}
			}
			using underlying_char_type = typename input_ref::input_char_type;
			::std::size_t units{range.end_offset - range.current_offset};
			::std::size_t const scratch_units{
				static_cast<::std::size_t>(scratch.buffer_capacity_end -
										   scratch.buffer_begin) /
				sizeof(underlying_char_type)};
			if (units > scratch_units)
			{
				// Limit the copied typed prefix to complete objects fitting scratch.
				units = scratch_units;
			}
			::std::size_t const bytes{units * sizeof(underlying_char_type)};
			::fast_io::freestanding::my_memcpy(
				scratch.buffer_begin, range.current, bytes);
			ibuffer_set_curr(inref, range.current + units);
			next = scratch.buffer_begin + bytes;
		}
		else
		{
			// A byte engine over an unbuffered source uses byte-read dispatch.
			static_assert(::std::same_as<unit_type, ::std::byte>);
			next = ::fast_io::operations::decay::read_some_bytes_decay_dispatch(
				inref, scratch.buffer_begin, scratch.buffer_capacity_end);
		}
		auto const validated_next{
			::fast_io::details::validate_transcode_closed_range_offsets(
				scratch.buffer_begin, scratch.buffer_capacity_end, next)};
		if (!validated_next.valid) [[unlikely]]
		{
			// A read customization may return an unrelated or misaligned cursor;
			// validate its integer offset before observing progress.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::protocol_violation);
		}
		next = validated_next.current;
		if (validated_next.current_offset == 0u)
		{
			// A zero-length successful read is interpreted as physical EOF.
			source_eof = true;
			scratch.buffer_curr = scratch.buffer_end = scratch.buffer_begin;
			return {};
		}
		scratch.buffer_curr = scratch.buffer_begin;
		scratch.buffer_end = next;
		return {scratch.buffer_curr, scratch.buffer_end, false};
	}
}

/** @brief Validates and commits exactly the engine-consumed source prefix. */
template <::fast_io::transcode_unit unit_type, typename input_ref>
[[nodiscard]] inline unit_type const *commit_transcode_source(
	input_ref &inref,
	::fast_io::details::basic_transcode_input_buffer_pointers<unit_type> &scratch,
	basic_transcode_source<unit_type> source, unit_type const *next)
{
	// No engine result may escape the source range supplied for that call. The
	// validator observes no pointer relation before proving an integer offset.
	auto const validated_next{
		::fast_io::details::validate_transcode_closed_range_offsets(
			source.first, source.last, next)};
	if (!validated_next.valid) [[unlikely]]
	{
		// Engine cursors must remain inside the exact range supplied to process.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	next = validated_next.current;
	if (source.direct)
	{
		// Advance the borrowed underlying get-area cursor only after validation.
		if constexpr (::std::same_as<
						  unit_type, typename input_ref::input_char_type> &&
					  ::fast_io::operations::decay::defines::
						  has_ibuffer_basic_operations<input_ref>)
		{
			// The direct marker is valid only for an exact typed ibuffer source.
			using cursor_type = decltype(ibuffer_curr(inref));
			if constexpr (::std::is_const_v<
								  ::std::remove_pointer_t<cursor_type>>)
			{
				// Preserve a const cursor type accepted by the underlying setter.
				ibuffer_set_curr(inref, next);
			}
			else
			{
				// Restore mutability only for a mutable underlying cursor API.
				ibuffer_set_curr(inref, const_cast<unit_type *>(next));
			}
		}
		else
		{
			// A direct marker on a scratch-backed source is an internal violation.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::protocol_violation);
		}
	}
	else
	{
		// Retain any unconsumed scratch suffix for the next process call.
		scratch.buffer_curr = const_cast<unit_type *>(next);
	}
	return next;
}

/** @brief Prepares a direct output range or reusable adapter-owned scratch. */
template <typename allocator_type, bool secure_clear,
		  ::fast_io::transcode_unit unit_type,
		  typename output_ref>
inline basic_transcode_destination<unit_type> prepare_transcode_destination(
	output_ref &outref,
	::fast_io::details::basic_transcode_buffer_pointers<unit_type> &scratch,
	::std::size_t minimum, ::std::size_t preferred)
{
	// Exact writable obuffers allow the engine to produce directly into the
	// device's put area. Byte binding and unbuffered outputs use local scratch.
	if (minimum == 0u)
	{
		// Normalize a defensive zero query to one writable engine unit.
		minimum = 1u;
	}
	if constexpr (::std::same_as<unit_type,
								 typename output_ref::output_char_type> &&
				  ::fast_io::operations::decay::defines::
					  has_obuffer_basic_operations<output_ref>)
	{
		// Prefer a matching writable put area to avoid an intermediate copy.
		auto const range{
			::fast_io::details::acquire_transcode_obuffer_range(outref)};
		if (range.current != nullptr &&
			range.end_offset - range.current_offset >= minimum)
		{
			// Borrow only a put area satisfying the complete minimum-capacity query.
			return {range.current, range.end, true};
		}
	}
	::fast_io::details::ensure_transcode_scratch<allocator_type, secure_clear>(
		scratch, minimum, preferred);
	return {scratch.buffer_begin, scratch.buffer_end, false};
}

/** @brief Validates and commits exactly the engine-produced destination prefix. */
template <::fast_io::transcode_unit unit_type, typename output_ref>
[[nodiscard]] inline unit_type *commit_transcode_destination(
	output_ref &outref, basic_transcode_destination<unit_type> destination,
	unit_type *next)
{
	// Direct output commits only the produced prefix. Scratch output is fully
	// forwarded before the same allocation can be reused by the engine.
	auto const validated_next{
		::fast_io::details::validate_transcode_closed_range_offsets(
			destination.first, destination.last, next)};
	if (!validated_next.valid) [[unlikely]]
	{
		// Reject engine cursors escaping the exact destination range supplied.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	next = validated_next.current;
	if constexpr (::std::same_as<unit_type,
								 typename output_ref::output_char_type> &&
				  ::fast_io::operations::decay::defines::
					  has_obuffer_basic_operations<output_ref>)
	{
		// Only exact-unit buffered outputs can carry a direct destination marker.
		if (destination.direct)
		{
			// Publish the produced prefix by advancing the underlying put cursor.
			obuffer_set_curr(outref, next);
			return next;
		}
	}
	if (next != destination.first)
	{
		// Forward only a nonempty scratch prefix to the underlying output.
		if constexpr (::std::same_as<unit_type,
									 typename output_ref::output_char_type>)
		{
			// Forward matching scratch units through typed output dispatch.
			::fast_io::operations::decay::write_all_decay_dispatch(
				outref, destination.first, next);
		}
		else
		{
			// Forward a byte endpoint through explicit byte output dispatch.
			static_assert(::std::same_as<unit_type, ::std::byte>);
			::fast_io::operations::decay::write_all_bytes_decay_dispatch(
				outref, destination.first, next);
		}
	}
	return next;
}

} // namespace fast_io::details

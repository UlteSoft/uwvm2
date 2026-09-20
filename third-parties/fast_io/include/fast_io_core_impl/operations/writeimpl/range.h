#pragma once

/*
 * Iterator/range output adaptation (primitive operation sublayer).
 *
 * Non-contiguous iterator sources are copied or type-punned into bounded
 * temporary blocks and forwarded to the normalized `write_all` machinery.
 * Contiguous cases retain the direct transfer path. This file owns iteration
 * and temporary-block progress only; printable conversion must already have
 * happened before these APIs are selected.
 * The public range owner is normalized and, when eligible, terminally finished
 * by one basic_output_operation_guard around the complete iteration.
 */

namespace fast_io
{

namespace details
{

/**
 * @brief Copies complete iterator-value representations into a bounded output buffer.
 *
 * @details `fromfirst` and `fromlast` synchronously borrow the iterator state owned by range dispatch.  Progress is
 * committed directly to that unique iterator object, while the returned cursor describes only initialized output
 * units.  Each iteration first proves that one complete value representation fits; therefore neither padding bytes
 * nor a partial source object can cross the buffer boundary.  Exact lvalue references are copied from the referred
 * object, whereas proxy references are materialized as `iter_value_t` before their representation is observed.
 */
template <typename FromItbg, typename FromIted, typename ToIter>
inline constexpr ToIter
bytes_copy_punning_impl(FromItbg &fromfirst, FromIted &fromlast, ToIter tofirst, ToIter tolast)
{
	using fromvaluetype = ::std::iter_value_t<FromItbg>;
	using tovaluetype = ::std::iter_value_t<ToIter>;
	constexpr ::std::size_t units_per_value{sizeof(fromvaluetype) / sizeof(tovaluetype)};
	static_assert(sizeof(fromvaluetype) % sizeof(tovaluetype) == 0u,
				  "a range value representation must contain an integral number of output units");
	while (fromfirst != fromlast && static_cast<::std::size_t>(tolast - tofirst) >= units_per_value)
	{
		// Representation copying is synchronous: a materialized proxy remains alive until every output unit is stored.
		auto copy_value_representation = [&](fromvaluetype const &source) constexpr {
			if (__builtin_is_constant_evaluated())
			{
				// Constant evaluation uses a same-size array of output units; no pointer reinterpretation is required.
				auto arr{
					::std::bit_cast<::fast_io::freestanding::array<tovaluetype, units_per_value>>(source)};
				::fast_io::details::non_overlapped_copy(arr.data(), arr.data() + arr.size(), tofirst);
			}
			else
			{
				// Runtime copying observes the source element object, never the iterator object's representation.
				::fast_io::freestanding::bytes_copy_n(
					reinterpret_cast<::std::byte const *>(::std::addressof(source)),
					sizeof(fromvaluetype), reinterpret_cast<::std::byte *>(tofirst));
			}
		};
		using fromreferencetype = ::std::iter_reference_t<FromItbg>;
		if constexpr (::std::is_lvalue_reference_v<fromreferencetype> &&
					  ::std::same_as<::std::remove_cvref_t<fromreferencetype>, fromvaluetype>)
		{
			copy_value_representation(*fromfirst);
		}
		else
		{
			static_assert(::std::constructible_from<fromvaluetype, fromreferencetype>,
						  "a non-addressable iterator proxy must materialize its declared value_type");
			fromvaluetype materialized(*fromfirst);
			copy_value_representation(materialized);
		}
		tofirst += units_per_value;
		++fromfirst;
	}
	return tofirst;
}

/** @brief Emits a multiblock iterator range through bounded scatter batches. */
template <::std::size_t blocksize, typename outstmtype, typename T1, typename T>
inline constexpr void write_all_iterator_decay_multiblock_common_impl(outstmtype &outsm, T1 **controller_first,
																	  T const *firstblock_curr, T const *firstblock_end,
																	  T1 **controller_last, T const *lastblock_first,
																	  T const *lastblock_curr)
{
	using output_char_type = typename outstmtype::output_char_type;
	using nocref = ::std::remove_cvref_t<T>;
	constexpr bool hasbytesop{
		fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outstmtype>};
	if constexpr (::std::same_as<nocref, output_char_type> || ::std::same_as<nocref, ::std::byte>)
	{
		// Directly representable blocks can populate scatter descriptors.
		if (controller_first == controller_last)
		{
			// A same-block range reduces to one contiguous typed write.
			::fast_io::operations::decay::write_all_decay_dispatch(outsm, firstblock_curr, lastblock_curr);
			return;
		}
		using scattertype = ::std::conditional_t<hasbytesop, io_scatter_t, basic_io_scatter_t<output_char_type>>;
		constexpr ::std::size_t multiplier{hasbytesop ? sizeof(nocref) : sizeof(nocref) / sizeof(output_char_type)};
		constexpr ::std::size_t scatternum{64};
		static_assert(1 < scatternum);
		constexpr ::std::size_t blockszbytes{blocksize * multiplier};
		scattertype scatters[scatternum];
		*scatters =
			scattertype{firstblock_curr, static_cast<::std::size_t>(firstblock_end - firstblock_curr) * multiplier};
		scattertype *scatterit{scatters + 1}, *scattered{scatters + scatternum};
		for (++controller_first; controller_first != controller_last; ++controller_first)
		{
			*scatterit = {*controller_first, blockszbytes};
			++scatterit;
			if (scatterit == scattered)
			{
				// Flush a full fixed-size scatter batch before reusing descriptors.
				if constexpr (hasbytesop)
				{
					// Byte-capable outputs consume descriptor lengths in bytes.
					::fast_io::operations::decay::scatter_write_all_bytes_decay_dispatch(outsm, scatters, scatternum);
				}
				else
				{
					// Typed-only outputs consume descriptors in output units.
					::fast_io::operations::decay::scatter_write_all_decay_dispatch(outsm, scatters, scatternum);
				}
				scatterit = scatters;
			}
		}
		*scatterit =
			scattertype{lastblock_first, static_cast<::std::size_t>(lastblock_curr - lastblock_first) * multiplier};
		++scatterit;
		if constexpr (hasbytesop)
		{
			// Flush the final partial descriptor batch through byte dispatch.
			::fast_io::operations::decay::scatter_write_all_bytes_decay_dispatch(
				outsm, scatters, static_cast<::std::size_t>(scatterit - scatters));
		}
		else
		{
			// Flush the final partial descriptor batch through typed dispatch.
			::fast_io::operations::decay::scatter_write_all_decay_dispatch(outsm, scatters,
																		   static_cast<::std::size_t>(scatterit - scatters));
		}
	}
	else
	{
		// Rebind representation-compatible blocks to the selected output domain.
		using type_ptr_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= ::std::conditional_t<hasbytesop, ::std::byte, output_char_type> **;
		using type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= ::std::conditional_t<hasbytesop, ::std::byte, output_char_type> const *;
		using type_const_ptr_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= type_const_ptr *;
		write_all_iterator_decay_multiblock_common_impl<blocksize>(
			outsm, const_cast<type_const_ptr_ptr>(reinterpret_cast<type_ptr_ptr>(controller_first)),
			reinterpret_cast<type_const_ptr>(firstblock_curr), reinterpret_cast<type_const_ptr>(firstblock_end),
			const_cast<type_const_ptr_ptr>(reinterpret_cast<type_ptr_ptr>(controller_last)),
			reinterpret_cast<type_const_ptr>(lastblock_first), reinterpret_cast<type_const_ptr>(lastblock_curr));
	}
}

/**
 * @brief Synchronously writes a borrowed non-contiguous iterator range under one synchronization scope.
 *
 * @details `first` and `last` denote the unique iterator/sentinel objects owned by the value-transport wrapper
 * below.  This implementation never lets either reference escape, so recursive mutex unwrapping preserves the
 * lifetime and exact expression category selected by the unlocked-stream CPO without reacquiring iterator
 * ownership.  In particular, mutex depth contributes O(1) iterator construction instead of one copy per layer,
 * and move-only input iterators remain valid protocol participants.
 */
template <typename outstmtype, typename Iter, typename Iterlast>
inline constexpr void write_all_iterator_decay_borrowed_impl(outstmtype &outsm, Iter &first, Iterlast &last)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		// A mutex-marked observer must be unwrapped before iterator decomposition.
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			// Hold one lock across every scalar or block write generated below.
			// Iteration may lower to several scalar writes. Holding the guard outside recursive range dispatch proves
			// those writes form one synchronized operation instead of one independently locked operation per block.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// `decltype(auto)` retains either an observer reference or an observer value exactly as returned by the CPO.
			// A value result is a named automatic object whose lifetime encloses the complete synchronous borrowed call.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_all_iterator_decay_borrowed_impl(unlocked, first, last);
		}
		else
		{
			// Diagnose an incomplete mutex protocol instead of recursing ambiguously.
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		// Select a direct multiblock or bounded scalar-copy strategy.
		using output_char_type = typename outstmtype::output_char_type;
		using itvt = ::std::iter_value_t<Iter>;
		constexpr bool use_typed_operations{
			::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_operations<outstmtype> &&
			(sizeof(itvt) % sizeof(output_char_type) == 0u)};
		using operation_unit_type =
			::std::conditional_t<use_typed_operations, output_char_type, ::std::byte>;
		if constexpr (::fast_io::multiblock_view_iterator<Iter>) // Optimize for ::std::deque
		{
			// Preserve container block structure and lower it to scatter batches.
			auto firstvit{multiblock_iterator_view_ref_define(first)};
			auto lastvit{multiblock_iterator_view_ref_define(last)};
			::fast_io::details::write_all_iterator_decay_multiblock_common_impl<decltype(firstvit)::block_size>(
				outsm, firstvit.controller_ptr, firstvit.block_curr_ptr, firstvit.block_end_ptr, lastvit.controller_ptr,
				lastvit.block_begin_ptr, lastvit.block_curr_ptr);
		}
		else
		{
			// Generic iterators use the typed domain only when its capability and representation divisibility are proven.
			// Otherwise the exact same object representation is transported through the byte-operation domain.
			constexpr ::std::size_t bfsz{(::std::numeric_limits<::std::size_t>::digits <= 16u ? 64u : 512u) /
										 sizeof(operation_unit_type)};
			if constexpr (sizeof(operation_unit_type) < sizeof(itvt) &&
						  sizeof(itvt) <= sizeof(operation_unit_type) * bfsz)
			{
				// Batch only complete representations that provably fit in the bounded buffer.
				operation_unit_type buffer[bfsz];
				for (; first != last;)
				{
					// The helper advances the unique borrowed iterator and returns only initialized output progress.
					auto toiter{
						::fast_io::details::bytes_copy_punning_impl(first, last, buffer, buffer + bfsz)};
					if constexpr (use_typed_operations)
					{
						::fast_io::operations::decay::write_all_decay_dispatch(outsm, buffer, toiter);
					}
					else
					{
						::fast_io::operations::decay::write_all_bytes_decay_dispatch(outsm, buffer, toiter);
					}
				}
			}
			else
			{
				// Single-unit and oversized values bypass staging; the latter cannot overflow the bounded buffer.
				for (; first != last; ++first)
				{
					// The consumer is synchronous, so an optional proxy materialization remains alive through the write CPO.
					auto write_one_representation = [&](itvt const &source) constexpr {
						if constexpr (use_typed_operations && ::std::same_as<output_char_type, itvt>)
						{
							// Matching values use a one-element typed write directly.
							auto firstaddr{::std::addressof(source)};
							::fast_io::operations::decay::write_all_decay_dispatch(outsm, firstaddr, firstaddr + 1);
						}
						else
						{
							// Representation-compatible values use an alias-safe output view.
							using type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
								[[__gnu__::__may_alias__]]
#endif
								= operation_unit_type const *;
							auto firstaddr{::std::addressof(source)};
							auto operation_first{reinterpret_cast<type_const_ptr>(firstaddr)};
							auto operation_last{reinterpret_cast<type_const_ptr>(firstaddr + 1)};
							if constexpr (use_typed_operations)
							{
								::fast_io::operations::decay::write_all_decay_dispatch(outsm, operation_first,
																					   operation_last);
							}
							else
							{
								::fast_io::operations::decay::write_all_bytes_decay_dispatch(outsm, operation_first,
																							 operation_last);
							}
						}
					};
					using iter_reference_type = ::std::iter_reference_t<Iter>;
					if constexpr (::std::is_lvalue_reference_v<iter_reference_type> &&
								  ::std::same_as<::std::remove_cvref_t<iter_reference_type>, itvt>)
					{
						write_one_representation(*first);
					}
					else
					{
						static_assert(::std::constructible_from<itvt, iter_reference_type>,
									  "a non-addressable iterator proxy must materialize its declared value_type");
						itvt materialized(*first);
						write_one_representation(materialized);
					}
				}
			}
		}
	}
}

/**
 * @brief Owns normalized iterator state once, then delegates to the synchronous borrowed implementation.
 *
 * @details Keeping this boundary by value preserves register-friendly ABI transport for ordinary iterators and
 * sentinels.  The borrowed implementation is the sole recursive path; consequently it cannot duplicate ownership
 * when a mutex observer is replaced by its unlocked observer.
 */
template <typename outstmtype, typename Iter, typename Iterlast>
inline constexpr void write_all_iterator_decay_impl(outstmtype &outsm, Iter first, Iterlast last)
{
	::fast_io::details::write_all_iterator_decay_borrowed_impl(outsm, first, last);
}

} // namespace details

namespace operations::decay
{

/// @brief Shares the complete output/range admission proof across all three transport entries.
/// @details Factoring this predicate keeps owner, borrow, and ABI dispatch as parameter-form choices over one operation;
///          no entry can accidentally accept a different byte/typed representation or iterator value category.
template <typename outstmtype, typename rg>
concept write_all_range_decay_compatible =
	::std::ranges::input_range<rg> &&
	((::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<
		  ::std::remove_cvref_t<outstmtype>> ||
	  (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_operations<
		   ::std::remove_cvref_t<outstmtype>> &&
	   (sizeof(::std::ranges::range_value_t<rg>) %
			sizeof(typename ::std::remove_cvref_t<outstmtype>::output_char_type) ==
		0))) &&
	 ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
		 ::std::ranges::range_value_t<rg>>);

/**
 * @brief Writes a normalized range while borrowing one stable output observer.
 *
 * @details Range value-category forwarding is independent from stream
 *          transport. The range expression is consumed synchronously, while
 *          mutex recursion and every terminal write observe this exact output
 *          object. Keeping the algorithm behind an explicitly borrowed name
 *          prevents an unlocked observer from reopening a value-copy edge.
 */
template <typename outstmtype, typename rg>
	requires ::fast_io::operations::decay::write_all_range_decay_compatible<
		outstmtype, rg>
inline constexpr void write_all_range_decay_borrowed_output(outstmtype &outsm, rg &&r)
{
	using normalized_outstmtype = ::std::remove_cvref_t<outstmtype>;
	using output_char_type = typename normalized_outstmtype::output_char_type;
	using rgvlt = ::std::ranges::range_value_t<rg>;
	if constexpr (::std::ranges::contiguous_range<rg>)
	{
		// Contiguous storage bypasses iterator staging entirely.
		auto firstptr{::std::ranges::cdata(r)};
		auto lastptr{::std::to_address(::std::ranges::cend(r))};
		if constexpr (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_operations<
						  normalized_outstmtype> &&
					  (sizeof(rgvlt) % sizeof(output_char_type) == 0))
		{
			// Prefer typed output when the representation divides into output units.
			if constexpr (::std::same_as<rgvlt, output_char_type>)
			{
				// Matching range values use their native contiguous pointers.
				::fast_io::operations::decay::write_all_decay_dispatch(outsm, firstptr, lastptr);
			}
			else
			{
				// Compatible nonmatching values use an alias-safe typed view.
				using type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= output_char_type const *;
				auto firstptrbt{reinterpret_cast<type_const_ptr>(firstptr)};
				auto lastptrbt{reinterpret_cast<type_const_ptr>(lastptr)};
				::fast_io::operations::decay::write_all_decay_dispatch(outsm, firstptrbt, lastptrbt);
			}
		}
		else
		{
			// Fall back to explicit byte dispatch for opaque representations.
			auto firstptrbt{reinterpret_cast<::std::byte const *>(firstptr)};
			auto lastptrbt{reinterpret_cast<::std::byte const *>(lastptr)};
			::fast_io::operations::decay::write_all_bytes_decay_dispatch(outsm, firstptrbt, lastptrbt);
		}
	}
	else
	{
		// Non-contiguous ranges are lowered through the iterator implementation.
		::fast_io::details::write_all_iterator_decay_impl(outsm, ::std::ranges::cbegin(r), ::std::ranges::cend(r));
	}
}

/**
 * @brief Owns the normalized output observer at the historical decay boundary.
 *
 * @details A genuine value parameter preserves the target aggregate ABI for
 *          explicit low-level calls. The range retains its incoming category,
 *          and the complete algorithm immediately borrows this one owner.
 */
template <typename outstmtype, typename rg>
	requires ::fast_io::operations::decay::write_all_range_decay_compatible<
		outstmtype, rg>
inline constexpr void write_all_range_decay(outstmtype outsm, rg &&r)
{
	::fast_io::operations::decay::write_all_range_decay_borrowed_output(
		outsm, ::std::forward<rg>(r));
}

/**
 * @brief Selects value or borrowed output transport without changing the range.
 *
 * @details The semantic substitution marker and the target ABI policy must
 *          both approve a value copy. Otherwise the named public observer is
 *          passed by reference, preserving inline cursor identity.
 */
template <typename outstmtype, typename rg>
	requires ::fast_io::operations::decay::write_all_range_decay_compatible<
		outstmtype, rg>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
write_all_range_decay_dispatch(outstmtype &outsm, rg &&r)
{
	if constexpr (
		::fast_io::operations::defines::abi_value_output_stream_ref_result<
			outstmtype &>)
	{
		::fast_io::operations::decay::write_all_range_decay(
			outsm, ::std::forward<rg>(r));
	}
	else
	{
		::fast_io::operations::decay::write_all_range_decay_borrowed_output(
			outsm, ::std::forward<rg>(r));
	}
}

} // namespace operations::decay

namespace operations
{

/** @brief Writes an entire range and checked-finishes an eligible temporary. */
template <typename outstmtype, ::std::ranges::input_range R>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all_range(outstmtype &&outstm, R &&r)
{
	::fast_io::operations::basic_output_operation_guard<outstmtype &&> guard{outstm};
	::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &outsm) {
		::fast_io::operations::decay::write_all_range_decay_dispatch(
			outsm, ::std::forward<R>(r));
	});
}

} // namespace operations

} // namespace fast_io

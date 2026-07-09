#pragma once

namespace fast_io
{

namespace details::decay
{

/// @brief    Reusable scatter descriptor for a single trailing newline.
/// @details  The void specialization reports byte count for byte-scatter APIs; typed scatter descriptors report
///           character count for character-scatter APIs.
/// @tparam   char_type the character type of the newline literal
/// @tparam   T         the scatter value type, or void for byte scatter output
template <::std::integral char_type, typename T = char_type>
inline constexpr basic_io_scatter_t<T> line_scatter_common{__builtin_addressof(char_literal_v<u8'\n', char_type>),
														   ::std::same_as<T, void> ? sizeof(char_type) : 1};

/// @brief    Describes the first contiguous print run that can be represented by scatters and reserve buffers.
struct contiguous_scatter_result
{
	::std::size_t position{};
	::std::size_t neededscatters{};
	::std::size_t neededspace{};
	::std::size_t null{};
	bool lastisreserve{};
	bool hasscatters{};
	bool hasreserve{};
	bool hasdynamicreserve{};
};

/// @brief    Scans a parameter pack for the leading contiguous scatter/reserve-friendly print run.
/// @details  The result records how many arguments can be grouped, how many scatter descriptors and reserve
///           characters are needed, and whether the run contains null or reserve-like outputs.
/// @tparam   char_type the character type used by the print run
/// @tparam   Arg       the current argument type
/// @tparam   Args      the remaining argument types
/// @return   contiguous_scatter_result the compile-time description of the leading run
template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr contiguous_scatter_result find_continuous_scatters_n()
{
	contiguous_scatter_result ret{};
	if constexpr (::fast_io::scatter_printable<char_type, Arg>)
	{
		// A scatter-printable argument contributes one existing output range to the contiguous run.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so the current argument can extend the leading run.
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		++ret.position;
		ret.hasscatters = true;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
	}
	else if constexpr (::fast_io::reserve_printable<char_type, Arg>)
	{
		// A static reserve-printable argument contributes one materialized range and a known reserve size.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first to preserve the aggregate run accounting.
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		else
		{
			// A trailing reserve argument can be emitted directly without an extra scatter continuation.
			ret.lastisreserve = true;
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(sz != 0);
		++ret.position;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
		ret.neededspace = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededspace, sz);
		ret.hasreserve = true;
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, Arg>)
	{
		// A dynamic reserve-printable argument is part of the run, but its reserve size is measured later.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so this dynamic reserve extends the same leading run.
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		else
		{
			// A trailing dynamic reserve argument can be handled as the final materialized output.
			ret.lastisreserve = true;
		}
		++ret.position;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
		ret.hasdynamicreserve = true;
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, Arg>)
	{
		// A reserve-scatters argument contributes its declared scatter count and reserve storage requirement.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first to accumulate the complete leading run.
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		constexpr auto scatszres{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(scatszres.scatters_size != 0);
		ret.hasscatters = true;
		ret.hasreserve = true;
		++ret.position;
		ret.neededspace =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededspace, scatszres.reserve_size);
		ret.neededscatters =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters, scatszres.scatters_size);
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, ::fast_io::io_null_t>)
	{
		// Null output is counted so pack positions remain correct while no emitted characters are reserved.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before adding this null position to the run.
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		++ret.position;
		ret.null = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.null,
																			 static_cast<::std::size_t>(1));
	}
	else if constexpr (::fast_io::printable<char_type, Arg>)
	{
		// A generic printable argument stops the scatter/reserve run because it needs the normal emit path.
	}
	return ret;
}

/// @brief    Describes a contiguous reserve-only or scatter-only prefix inside a print run.
struct scatter_rsv_result
{
	::std::size_t position{};
	::std::size_t neededspace{};
	::std::size_t null{};
};

/// @brief    Finds a contiguous reserve or scatter subrun used while building scatter descriptors.
/// @details  The findscatter flag selects whether the scan accepts scatter-printable arguments or reserve-printable
///           arguments, while nulls are counted and skipped in either mode.
/// @tparam   findscatter true to scan scatter-printable arguments, false to scan reserve-printable arguments
/// @tparam   char_type   the character type used by the print run
/// @tparam   Arg         the current argument type
/// @tparam   Args        the remaining argument types
/// @return   scatter_rsv_result the accepted run length, reserve space, and null count
template <bool findscatter, ::std::integral char_type, typename Arg, typename... Args>
inline constexpr scatter_rsv_result find_continuous_scatters_reserve_n()
{
	if constexpr (::fast_io::reserve_printable<char_type, Arg> && !findscatter)
	{
		// Reserve scan mode accepts static reserve-printable arguments and accumulates their reserve space.
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		if constexpr (sizeof...(Args) == 0)
		{
			// A single reserve argument forms a one-position run with its known reserve size.
			return {1, sz, 0};
		}
		else
		{
			// Additional arguments extend the reserve run while preserving accumulated nulls.
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, ::fast_io::details::intrinsics::add_or_overflow_die_chain(res.neededspace, sz), res.null};
		}
	}
	else if constexpr (::fast_io::scatter_printable<char_type, Arg> && findscatter)
	{
		// Scatter scan mode accepts scatter-printable arguments without reserving extra character storage.
		if constexpr (sizeof...(Args) == 0)
		{
			// A single scatter argument forms a one-position run with no reserve storage.
			return {1, 0, 0};
		}
		else
		{
			// Additional arguments extend the scatter run and keep the existing reserve accounting.
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, res.neededspace, res.null};
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, ::fast_io::io_null_t>)
	{
		// Null output is accepted in both scan modes because it does not interrupt contiguous emission.
		if constexpr (sizeof...(Args) == 0)
		{
			// A single null argument contributes one position and one null count.
			return {1, 0, 1};
		}
		else
		{
			// Additional arguments extend the run while the null count records this skipped output slot.
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, res.neededspace, ::fast_io::details::intrinsics::add_or_overflow_die_chain(res.null, static_cast<::std::size_t>(1))};
		}
	}
	else
	{
		// Any non-matching argument terminates the selected contiguous scatter or reserve subrun.
		return {0, 0, 0};
	}
}

/// @brief    Checks whether a stream's minimum output buffer size is strictly larger than N.
template <typename output, ::std::size_t N>
inline constexpr bool minimum_buffer_output_stream_require_size_constant_impl =
	(N < obuffer_minimum_size_define(::fast_io::io_reserve_type<typename output::output_char_type, output>));

/// @brief    Detects output streams that declare a minimum put area larger than N.
template <typename output, ::std::size_t N>
concept minimum_buffer_output_stream_require_size_impl =
	::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<output> &&
	minimum_buffer_output_stream_require_size_constant_impl<output, N>;

/// @brief    Default maximum stack-buffer budget used by freestanding print helpers.
inline constexpr ::std::size_t print_stack_buffer_default_max_bytes{
	::fast_io::details::print_stack_buffer_default_max_bytes};

using default_print_stack_policy = ::fast_io::details::default_print_stack_policy;

/// @brief    Stack-buffer policy alias for freestanding print materialization.
/// @tparam   requested_max_bytes requested maximum stack bytes for the policy
template <::std::size_t requested_max_bytes>
using print_stack_policy = ::fast_io::details::print_stack_policy<requested_max_bytes>;

/// @brief    Returns the maximum stack-buffer budget in bytes for a print stack policy.
/// @tparam   stack_policy the stack-buffer policy type
/// @return   ::std::size_t the maximum number of stack bytes available to print helpers
template <typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t print_stack_buffer_max_bytes() noexcept
{
	return ::fast_io::details::print_stack_buffer_max_bytes<stack_policy>();
}

/// @brief    Converts a byte stack-buffer budget into an element count for a concrete element type.
/// @tparam   element_type the element stored in the stack buffer
/// @tparam   stack_policy the stack-buffer policy type
/// @return   ::std::size_t the maximum number of elements that fit in the policy budget
template <typename element_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t print_stack_buffer_max_element_count() noexcept
{
	constexpr ::std::size_t max_bytes{::fast_io::details::decay::print_stack_buffer_max_bytes<stack_policy>()};
	if constexpr (max_bytes < sizeof(element_type))
	{
		// The policy budget cannot hold even one element, so stack materialization is disabled.
		return 0u;
	}
	else
	{
		// The policy budget is expressed in bytes and is rounded down to a whole element count.
		return max_bytes / sizeof(element_type);
	}
}

/// @brief    Returns the maximum character count for stack buffers in the print path.
/// @tparam   char_type    the character type stored in the buffer
/// @tparam   stack_policy the stack-buffer policy type
/// @return   ::std::size_t the maximum number of characters available on the stack
template <::std::integral char_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t print_stack_buffer_max_size() noexcept
{
	return ::fast_io::details::decay::print_stack_buffer_max_element_count<char_type, stack_policy>();
}

/// @brief    Tests whether a requested fixed-size stack buffer is permitted by the print stack policy.
/// @tparam   requested_size the requested element count
/// @tparam   element_type   the element stored in the stack buffer
/// @tparam   stack_policy   the stack-buffer policy type
template <::std::size_t requested_size, typename element_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr bool print_stack_buffer_size_within_limit =
	requested_size != 0 &&
	requested_size <= ::fast_io::details::decay::print_stack_buffer_max_element_count<element_type, stack_policy>();

/// @brief    Clamps a dynamic reserve producer's static stack hint to the active print stack budget.
/// @details  Dynamic producers may appear many times in one print run, so their local stack hints are bounded by the
///           print policy before a shared stack materialization path uses them.
/// @tparam   static_stack_size the producer-declared static stack size
/// @tparam   char_type         the character type stored in the stack buffer
/// @tparam   stack_policy      the stack-buffer policy type
/// @return   ::std::size_t     the stack size allowed for this producer
template <::std::size_t static_stack_size, ::std::integral char_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t dynamic_print_reserve_static_stack_budget() noexcept
{
	/*
	A dynamic producer's static stack size is only a local materialization hint.
	A print run can contain many dynamic producers, so directly merging all hints into one
	stack array can create a large frame and defeat the stack-safety purpose of this path.
	*/
	constexpr ::std::size_t run_cap{::fast_io::details::decay::print_stack_buffer_max_size<char_type, stack_policy>()};
	if constexpr (static_stack_size < run_cap)
	{
		// The producer's hint already fits inside the active stack-buffer policy.
		return static_stack_size;
	}
	else
	{
		// The producer's hint is capped so one dynamic output cannot dominate the stack frame.
		return run_cap;
	}
}

/// @brief    Computes the bounded static stack size for one dynamic reserve-printable argument.
/// @tparam   line         true when a trailing newline must fit in the same materialization buffer
/// @tparam   char_type    the character type stored in the stack buffer
/// @tparam   T            the dynamic reserve-printable argument type
/// @tparam   stack_policy the stack-buffer policy type
/// @return   ::std::size_t the bounded stack size, or zero when no static hint is available
template <bool line, ::std::integral char_type, typename T, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t dynamic_print_reserve_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		// Dynamic reserve producers with a static hint can use bounded stack materialization.
		constexpr ::std::size_t static_stack_size{
			print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		static_assert(!line || static_stack_size != SIZE_MAX);
		return ::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<static_stack_size, char_type, stack_policy>() +
			   static_cast<::std::size_t>(line);
	}
	else
	{
		// Producers without a static hint use the non-static dynamic reserve path.
		return 0;
	}
}

/// @brief    Returns the static context buffer size for one context-printable argument.
/// @details  Context-printable types may declare a fixed buffer size; otherwise this path uses the conservative
///           default used by the context printer.
/// @tparam   line      true when a trailing newline is included in the same context buffer
/// @tparam   char_type the character type stored in the context buffer
/// @tparam   T         the context-printable argument type
/// @return   ::std::size_t the static context buffer size in characters
template <bool line, ::std::integral char_type, typename T>
inline constexpr ::std::size_t context_print_static_buffer_size() noexcept
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, value_type>)
	{
		// A declared static context buffer size is validated before it drives stack allocation.
		constexpr ::std::size_t n{
			print_context_static_buffer_size(::fast_io::io_reserve_type<char_type, value_type>)};
		static_assert(n != 0);
		static_assert(n != SIZE_MAX);
		static_assert(n > static_cast<::std::size_t>(line));
		return n;
	}
	else
	{
		// Context producers without a declared size use the historical small default buffer.
		return 32u;
	}
}

/// @brief    Cached context buffer size for a context-printable argument.
/// @tparam   line      true when a trailing newline is included in the same context buffer
/// @tparam   char_type the character type stored in the context buffer
/// @tparam   T         the context-printable argument type
template <bool line, ::std::integral char_type, typename T>
inline constexpr ::std::size_t context_print_static_buffer_size_v{
	::fast_io::details::decay::context_print_static_buffer_size<line, char_type, T>()};

/// @brief    Converts character scatter lengths into byte scatter lengths when char_type is wider than one byte.
/// @tparam   sz    sizeof(char_type), constrained to be non-zero
/// @param    first the first scatter descriptor to update
/// @param    last  one past the final scatter descriptor to update
template <::std::size_t sz>
	requires(sz != 0)
inline constexpr void scatter_rsv_update_times(::fast_io::io_scatter_t *first, ::fast_io::io_scatter_t *last) noexcept
{
	if constexpr (sz != 1)
	{
		// Wide-character reserve scatters must report byte counts to byte-oriented scatter APIs.
		for (; first != last; ++first)
		{
			first->len *= sz;
		}
	}
}

/// @brief    Result of building byte-oriented reserve-scatters output.
/// @tparam   char_type the character type stored in the temporary reserve buffer
template <::std::integral char_type>
struct basic_reserve_scatters_define_byte_result
{
	io_scatter_t *scatters_pos_ptr;
	char_type *reserve_pos_ptr;
};

/// @brief    Builds reserve-scatters output for byte-oriented scatter APIs.
/// @details  The typed scatter descriptors returned by the customization are reinterpreted as byte scatter
///           descriptors and length-adjusted for wide character types.
/// @tparam   char_type the character type stored in the temporary reserve buffer
/// @tparam   T         the reserve-scatters printable argument type
/// @param    pscatters the byte scatter descriptor storage
/// @param    buffer    the temporary reserve buffer
/// @param    t         the argument to materialize
/// @return   basic_reserve_scatters_define_byte_result<char_type> updated scatter and reserve-buffer cursors
template <::std::integral char_type, typename T>
inline ::fast_io::details::decay::basic_reserve_scatters_define_byte_result<char_type>
prrsvsct_byte_common_rsvsc_impl(io_scatter_t *pscatters, char_type *buffer, T t)
{
	using basicioscattertypealiasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<char_type> *;
	using scatterioscattertypealiasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t *;
	auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
											  reinterpret_cast<basicioscattertypealiasptr>(pscatters), buffer, t)};
	scatterioscattertypealiasptr ptr{reinterpret_cast<scatterioscattertypealiasptr>(result.scatters_pos_ptr)};
	if constexpr (sizeof(char_type) != 1)
	{
		// Wide-character scatter lengths are converted from character counts to byte counts.
		scatter_rsv_update_times<sizeof(char_type)>(pscatters, ptr);
	}
	return {ptr, result.reserve_pos_ptr};
}

/// @brief    Builds byte-oriented reserve-scatters output and returns the final scatter cursor.
/// @tparam   char_type the character type stored in the temporary reserve buffer
/// @tparam   T         the reserve-scatters printable argument type
/// @param    pscatters the byte scatter descriptor storage
/// @param    buffer    the temporary reserve buffer
/// @param    t         the argument to materialize
/// @return   io_scatter_t* one past the final scatter descriptor
template <::std::integral char_type, typename T>
inline auto prrsvsct_byte_common_impl(io_scatter_t *pscatters, char_type *buffer, T t)
{
	return ::fast_io::details::decay::prrsvsct_byte_common_rsvsc_impl(pscatters, buffer, t).scatters_pos_ptr;
}

/// @brief    Returns the maximum scatter fallback coalescing threshold in characters.
/// @tparam   char_type the output character type
/// @return   ::std::size_t the stack-buffer character limit for scatter fallback coalescing
template <::std::integral char_type>
inline constexpr ::std::size_t print_scatter_full_output_threshold_max_chars() noexcept
{
	return ::fast_io::details::decay::print_stack_buffer_max_size<char_type>();
}

/// @brief    Computes the scatter fallback coalescing threshold for an output stream.
/// @details  A stream-provided threshold is capped by the active stack-buffer policy; streams without the customization
///           disable scatter fallback coalescing.
/// @tparam   char_type     the output character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @return   ::std::size_t the threshold in characters, or zero when disabled
template <::std::integral char_type, typename outputstmtype>
inline constexpr ::std::size_t print_scatter_full_output_threshold() noexcept
{
	if constexpr (::fast_io::scatter_fallback_full_output_threshold_stream<char_type, outputstmtype>)
	{
		// Stream-specific scatter fallback thresholds are honored but never allowed to exceed the stack budget.
		constexpr ::std::size_t threshold{
			scatter_fallback_full_output_threshold(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<outputstmtype>>)};
		constexpr ::std::size_t max_threshold{
			::fast_io::details::decay::print_scatter_full_output_threshold_max_chars<char_type>()};
		if constexpr (threshold < max_threshold)
		{
			// The stream threshold already fits within the stack-buffer policy.
			return threshold;
		}
		else
		{
			// The stack-buffer policy caps oversized stream thresholds.
			return max_threshold;
		}
	}
	else
	{
		// Streams without a scatter fallback threshold do not use stack coalescing for scatter fallback.
		return 0u;
	}
}

/// @brief    Computes the full-output coalescing threshold for an output stream.
/// @details  Full-output coalescing has its own stream customization and falls back to the scatter fallback threshold
///           only when the full-output customization is absent.
/// @tparam   char_type     the output character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @return   ::std::size_t the threshold in characters, or zero when both threshold paths are disabled
template <::std::integral char_type, typename outputstmtype>
inline constexpr ::std::size_t print_full_output_coalesce_threshold() noexcept
{
	if constexpr (::fast_io::full_output_coalesce_threshold_stream<char_type, outputstmtype>)
	{
		// A stream-specific full-output threshold is capped by the same stack-buffer policy as scatter fallback.
		constexpr ::std::size_t threshold{
			full_output_coalesce_threshold(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<outputstmtype>>)};
		constexpr ::std::size_t max_threshold{
			::fast_io::details::decay::print_scatter_full_output_threshold_max_chars<char_type>()};
		if constexpr (threshold < max_threshold)
		{
			// The stream threshold already fits within the stack-buffer policy.
			return threshold;
		}
		else
		{
			// The stack-buffer policy caps oversized stream thresholds.
			return max_threshold;
		}
	}
	else
	{
		// Without a full-output threshold, preserve the scatter fallback threshold behavior.
		return ::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>();
	}
}

/// @brief    Detects whether an output stream can write a contiguous byte range directly.
/// @tparam   outputstmtype the decayed output stream reference type
template <typename outputstmtype>
inline constexpr bool print_has_direct_write_bytes_operations =
	::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_write_all_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_write_some_overflow_define<outputstmtype>));

/// @brief    Detects whether an output stream can write byte scatter descriptors directly.
/// @tparam   outputstmtype the decayed output stream reference type
template <typename outputstmtype>
inline constexpr bool print_has_direct_scatter_write_bytes_operations =
	::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outputstmtype>));

/// @brief    Detects whether an output stream can write a contiguous character range directly.
/// @tparam   outputstmtype the decayed output stream reference type
template <typename outputstmtype>
inline constexpr bool print_has_direct_write_operations =
	::fast_io::operations::decay::defines::has_write_all_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_write_some_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outputstmtype>));

/// @brief    Detects whether an output stream can write character scatter descriptors directly.
/// @tparam   outputstmtype the decayed output stream reference type
template <typename outputstmtype>
inline constexpr bool print_has_direct_scatter_write_operations =
	::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outputstmtype>));

/// @brief    Copies a small scatter payload into a contiguous coalescing buffer.
/// @details  Very small ranges are copied with a simple loop to avoid helper overhead; larger ranges use the normal
///           non-overlapped copy routine.
/// @tparam   char_type the element type being copied
/// @param    first     the first input element
/// @param    count     the number of elements to copy
/// @param    result    the first output element
/// @return   char_type* one past the copied output
template <typename char_type>
inline constexpr char_type *print_small_scatter_copy_n(char_type const *first, ::std::size_t count,
													   char_type *result) noexcept
{
	if (count <= 16u)
	{
		// Tiny scatter ranges are copied inline to keep the coalescing hot path compact.
		for (::std::size_t i{}; i != count; ++i)
		{
			result[i] = first[i];
		}
		return result + count;
	}
	// Larger scatter ranges use the shared non-overlapped copy implementation.
	return ::fast_io::details::non_overlapped_copy_n(first, count, result);
}

template <::std::integral char_type, typename outputstmtype>
inline constexpr ::std::size_t print_small_scatter_repack_size() noexcept
{
	if constexpr (::fast_io::small_scatter_coalesce_threshold_stream<char_type, outputstmtype>)
	{
		return small_scatter_coalesce_threshold(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<outputstmtype>>);
	}
	else
	{
		return 0u;
	}
}

inline constexpr ::std::size_t print_scatter_repack_min_saved_scatters{8u};

template <typename scatter_type>
inline constexpr ::std::size_t print_scatter_repack_stack_scatter_count() noexcept
{
	constexpr ::std::size_t stack_count{
		::fast_io::details::decay::print_stack_buffer_max_element_count<scatter_type>()};
	if constexpr (stack_count < 64u)
	{
		return stack_count;
	}
	else
	{
		return 64u;
	}
}

template <typename outputstmtype>
inline constexpr bool print_scatter_write_all_bytes_try_repack_small(
	outputstmtype outstm, ::fast_io::io_scatter_t const *scatters, ::std::size_t n)
{
	using char_type = typename outputstmtype::output_char_type;
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	constexpr ::std::size_t small_chars{
		::fast_io::details::decay::print_small_scatter_repack_size<char_type, outputstmtype>()};
	if constexpr (threshold_chars == 0 || small_chars == 0 ||
				  !::fast_io::details::decay::print_has_direct_scatter_write_bytes_operations<outputstmtype>)
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t chunk_bytes{threshold_chars * sizeof(char_type) - 1u};
		constexpr ::std::size_t small_bytes{small_chars * sizeof(char_type)};
		if constexpr (chunk_bytes == 0)
		{
			return false;
		}
		else
		{
			::std::size_t output_count{};
			::std::size_t dynamic_buffer_size{};
			::std::size_t current_size{};
			::std::size_t current_count{};
			bool stack_chunk_used{};
			bool has_coalesced_chunk{};
			auto flush_plan = [&]() constexpr {
				if (current_count == 0)
				{
					return;
				}
				++output_count;
				if (1u < current_count)
				{
					has_coalesced_chunk = true;
					if (stack_chunk_used)
					{
						dynamic_buffer_size =
							::fast_io::details::intrinsics::add_or_overflow_die(dynamic_buffer_size, current_size);
					}
					else
					{
						stack_chunk_used = true;
					}
				}
				current_size = 0;
				current_count = 0;
			};
			for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
			{
				::std::size_t const len{iter->len};
				if (len == 0)
				{
					continue;
				}
				if (small_bytes < len)
				{
					flush_plan();
					++output_count;
					continue;
				}
				if (chunk_bytes - current_size < len)
				{
					flush_plan();
				}
				current_size += len;
				++current_count;
			}
			flush_plan();
			if (!has_coalesced_chunk || output_count > n ||
				n - output_count < print_scatter_repack_min_saved_scatters)
			{
				return false;
			}

			::std::byte stack_buffer[chunk_bytes];
			::fast_io::details::local_operator_new_array_ptr<::std::byte> dynamic_buffer;
			if (dynamic_buffer_size != 0)
			{
				dynamic_buffer.allocate_new(dynamic_buffer_size);
			}

			auto emit_repacked = [&](::fast_io::io_scatter_t *out_scatters) constexpr {
				auto out_curr{out_scatters};
				::std::byte *dynamic_curr{dynamic_buffer.ptr};
				::std::byte *chunk_begin{};
				::std::byte *chunk_curr{};
				::std::size_t chunk_size{};
				::std::size_t chunk_count{};
				bool chunk_is_stack{};
				bool stack_used{};
				::fast_io::io_scatter_t pending{};
				auto begin_chunk = [&]() constexpr {
					if (!stack_used)
					{
						chunk_begin = stack_buffer;
						chunk_curr = stack_buffer;
						chunk_is_stack = true;
						stack_used = true;
					}
					else
					{
						chunk_begin = dynamic_curr;
						chunk_curr = dynamic_curr;
						chunk_is_stack = false;
					}
				};
				auto flush_chunk = [&]() constexpr {
					if (chunk_count == 0)
					{
						return;
					}
					if (chunk_count == 1)
					{
						*out_curr = pending;
						++out_curr;
					}
					else
					{
						*out_curr = {chunk_begin, chunk_size};
						++out_curr;
						if (!chunk_is_stack)
						{
							dynamic_curr = chunk_curr;
						}
					}
					chunk_begin = {};
					chunk_curr = {};
					chunk_size = 0;
					chunk_count = 0;
					chunk_is_stack = false;
				};
				for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
				{
					::std::size_t const len{iter->len};
					if (len == 0)
					{
						continue;
					}
					if (small_bytes < len)
					{
						flush_chunk();
						*out_curr = *iter;
						++out_curr;
						continue;
					}
					if (chunk_bytes - chunk_size < len)
					{
						flush_chunk();
					}
					if (chunk_count == 0)
					{
						pending = *iter;
						chunk_size = len;
						chunk_count = 1;
						continue;
					}
					if (chunk_count == 1)
					{
						begin_chunk();
						auto const first{static_cast<::std::byte const *>(pending.base)};
						chunk_curr = ::fast_io::details::decay::print_small_scatter_copy_n(first, pending.len,
																						   chunk_curr);
					}
					auto const first{static_cast<::std::byte const *>(iter->base)};
					chunk_curr = ::fast_io::details::decay::print_small_scatter_copy_n(first, len, chunk_curr);
					chunk_size += len;
					++chunk_count;
				}
				flush_chunk();
				::std::size_t const repacked_count{static_cast<::std::size_t>(out_curr - out_scatters)};
				if constexpr (::fast_io::details::decay::print_has_direct_write_bytes_operations<outputstmtype>)
				{
					if (repacked_count == 1)
					{
						auto first{static_cast<::std::byte const *>(out_scatters->base)};
						::fast_io::operations::decay::write_all_bytes_decay(outstm, first,
																			first + out_scatters->len);
						return;
					}
				}
				::fast_io::operations::decay::scatter_write_all_bytes_decay(outstm, out_scatters, repacked_count);
			};

			constexpr ::std::size_t stack_scatter_count{
				::fast_io::details::decay::print_scatter_repack_stack_scatter_count<::fast_io::io_scatter_t>()};
			if constexpr (stack_scatter_count != 0)
			{
				if (output_count <= stack_scatter_count)
				{
					::fast_io::io_scatter_t out_scatters[stack_scatter_count];
					emit_repacked(out_scatters);
					return true;
				}
			}
			::fast_io::details::local_operator_new_array_ptr<::fast_io::io_scatter_t> out_scatters(output_count);
			emit_repacked(out_scatters.ptr);
			return true;
		}
	}
}

template <typename outputstmtype>
inline constexpr bool print_scatter_write_all_try_repack_small(
	outputstmtype outstm,
	::fast_io::basic_io_scatter_t<typename outputstmtype::output_char_type> const *scatters, ::std::size_t n)
{
	using char_type = typename outputstmtype::output_char_type;
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	constexpr ::std::size_t small_chars{
		::fast_io::details::decay::print_small_scatter_repack_size<char_type, outputstmtype>()};
	if constexpr (threshold_chars == 0 || small_chars == 0 ||
				  !::fast_io::details::decay::print_has_direct_scatter_write_operations<outputstmtype>)
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t chunk_chars{threshold_chars - 1u};
		if constexpr (chunk_chars == 0)
		{
			return false;
		}
		else
		{
			using scatter_type = ::fast_io::basic_io_scatter_t<char_type>;
			::std::size_t output_count{};
			::std::size_t dynamic_buffer_size{};
			::std::size_t current_size{};
			::std::size_t current_count{};
			bool stack_chunk_used{};
			bool has_coalesced_chunk{};
			auto flush_plan = [&]() constexpr {
				if (current_count == 0)
				{
					return;
				}
				++output_count;
				if (1u < current_count)
				{
					has_coalesced_chunk = true;
					if (stack_chunk_used)
					{
						dynamic_buffer_size =
							::fast_io::details::intrinsics::add_or_overflow_die(dynamic_buffer_size, current_size);
					}
					else
					{
						stack_chunk_used = true;
					}
				}
				current_size = 0;
				current_count = 0;
			};
			for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
			{
				::std::size_t const len{iter->len};
				if (len == 0)
				{
					continue;
				}
				if (small_chars < len)
				{
					flush_plan();
					++output_count;
					continue;
				}
				if (chunk_chars - current_size < len)
				{
					flush_plan();
				}
				current_size += len;
				++current_count;
			}
			flush_plan();
			if (!has_coalesced_chunk || output_count > n ||
				n - output_count < print_scatter_repack_min_saved_scatters)
			{
				return false;
			}

			char_type stack_buffer[chunk_chars];
			::fast_io::details::local_operator_new_array_ptr<char_type> dynamic_buffer;
			if (dynamic_buffer_size != 0)
			{
				dynamic_buffer.allocate_new(dynamic_buffer_size);
			}

			auto emit_repacked = [&](scatter_type *out_scatters) constexpr {
				auto out_curr{out_scatters};
				char_type *dynamic_curr{dynamic_buffer.ptr};
				char_type *chunk_begin{};
				char_type *chunk_curr{};
				::std::size_t chunk_size{};
				::std::size_t chunk_count{};
				bool chunk_is_stack{};
				bool stack_used{};
				scatter_type pending{};
				auto begin_chunk = [&]() constexpr {
					if (!stack_used)
					{
						chunk_begin = stack_buffer;
						chunk_curr = stack_buffer;
						chunk_is_stack = true;
						stack_used = true;
					}
					else
					{
						chunk_begin = dynamic_curr;
						chunk_curr = dynamic_curr;
						chunk_is_stack = false;
					}
				};
				auto flush_chunk = [&]() constexpr {
					if (chunk_count == 0)
					{
						return;
					}
					if (chunk_count == 1)
					{
						*out_curr = pending;
						++out_curr;
					}
					else
					{
						*out_curr = {chunk_begin, chunk_size};
						++out_curr;
						if (!chunk_is_stack)
						{
							dynamic_curr = chunk_curr;
						}
					}
					chunk_begin = {};
					chunk_curr = {};
					chunk_size = 0;
					chunk_count = 0;
					chunk_is_stack = false;
				};
				for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
				{
					::std::size_t const len{iter->len};
					if (len == 0)
					{
						continue;
					}
					if (small_chars < len)
					{
						flush_chunk();
						*out_curr = *iter;
						++out_curr;
						continue;
					}
					if (chunk_chars - chunk_size < len)
					{
						flush_chunk();
					}
					if (chunk_count == 0)
					{
						pending = *iter;
						chunk_size = len;
						chunk_count = 1;
						continue;
					}
					if (chunk_count == 1)
					{
						begin_chunk();
						chunk_curr = ::fast_io::details::decay::print_small_scatter_copy_n(
							pending.base, pending.len, chunk_curr);
					}
					chunk_curr =
						::fast_io::details::decay::print_small_scatter_copy_n(iter->base, len, chunk_curr);
					chunk_size += len;
					++chunk_count;
				}
				flush_chunk();
				::std::size_t const repacked_count{static_cast<::std::size_t>(out_curr - out_scatters)};
				if constexpr (::fast_io::details::decay::print_has_direct_write_operations<outputstmtype>)
				{
					if (repacked_count == 1)
					{
						auto [base, len] = *out_scatters;
						::fast_io::operations::decay::write_all_decay(outstm, base, base + len);
						return;
					}
				}
				::fast_io::operations::decay::scatter_write_all_decay(outstm, out_scatters, repacked_count);
			};

			constexpr ::std::size_t stack_scatter_count{
				::fast_io::details::decay::print_scatter_repack_stack_scatter_count<scatter_type>()};
			if constexpr (stack_scatter_count != 0)
			{
				if (output_count <= stack_scatter_count)
				{
					scatter_type out_scatters[stack_scatter_count];
					emit_repacked(out_scatters);
					return true;
				}
			}
			::fast_io::details::local_operator_new_array_ptr<scatter_type> out_scatters(output_count);
			emit_repacked(out_scatters.ptr);
			return true;
		}
	}
}

/// @brief    Writes byte scatter descriptors, optionally coalescing small scatter runs first.
/// @details  Empty and single-scatter runs are handled directly. Multi-scatter runs are copied into a bounded stack
///           buffer only when the stream supports both direct byte writes and direct byte scatter writes.
/// @tparam   outputstmtype the decayed output stream reference type
/// @param    outstm        the output stream reference
/// @param    scatters      the first byte scatter descriptor
/// @param    n             the number of scatter descriptors
template <typename outputstmtype>
inline constexpr void print_scatter_write_all_bytes_maybe_coalesce(outputstmtype outstm,
																   ::fast_io::io_scatter_t const *scatters,
																   ::std::size_t n)
{
	if (n == 0)
	{
		// An empty byte scatter run emits no output.
		return;
	}
	if (n == 1)
	{
		// A single byte scatter avoids scatter-write overhead and writes the range directly when non-empty.
		::std::size_t const len{scatters->len};
		if (len != 0)
		{
			// The single byte scatter contains payload bytes that can be written contiguously.
			auto first{static_cast<::std::byte const *>(scatters->base)};
			::fast_io::operations::decay::write_all_bytes_decay(outstm, first, first + len);
		}
		return;
	}
	using char_type = typename outputstmtype::output_char_type;
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	if constexpr (threshold_chars != 0 &&
				  ::fast_io::details::decay::print_has_direct_write_bytes_operations<outputstmtype> &&
				  ::fast_io::details::decay::print_has_direct_scatter_write_bytes_operations<outputstmtype>)
	{
		// Eligible multi-scatter byte runs first try bounded stack coalescing before using scatter write.
		char_type buffer[threshold_chars];
		auto curr{reinterpret_cast<::std::byte *>(buffer)};
		auto const first{curr};
		::std::size_t remaining{threshold_chars * sizeof(char_type) - 1u};
		for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
		{
			::std::size_t const len{iter->len};
			if (remaining < len)
			{
				// The byte run exceeds the coalescing threshold, so preserve scatter output directly.
				if (::fast_io::details::decay::print_scatter_write_all_bytes_try_repack_small(outstm, scatters, n))
				{
					return;
				}
				::fast_io::operations::decay::scatter_write_all_bytes_decay(outstm, scatters, n);
				return;
			}
			if (len != 0)
			{
				// Non-empty byte scatter payloads are appended to the contiguous coalescing buffer.
				curr = ::fast_io::details::decay::print_small_scatter_copy_n(
					static_cast<::std::byte const *>(iter->base), len, curr);
				remaining -= len;
			}
		}
		if (curr != first)
		{
			// At least one byte payload was coalesced, so emit the compact contiguous byte range.
			::fast_io::operations::decay::write_all_bytes_decay(outstm, first, curr);
		}
		return;
	}
	// Streams that cannot coalesce this path emit the original byte scatter descriptors directly.
	if (::fast_io::details::decay::print_scatter_write_all_bytes_try_repack_small(outstm, scatters, n))
	{
		return;
	}
	::fast_io::operations::decay::scatter_write_all_bytes_decay(outstm, scatters, n);
}

/// @brief    Writes character scatter descriptors, optionally coalescing small scatter runs first.
/// @details  This mirrors the byte-scatter path but preserves character counts and uses character-range output APIs.
/// @tparam   outputstmtype the decayed output stream reference type
/// @param    outstm        the output stream reference
/// @param    scatters      the first character scatter descriptor
/// @param    n             the number of scatter descriptors
template <typename outputstmtype>
inline constexpr void print_scatter_write_all_maybe_coalesce(
	outputstmtype outstm, ::fast_io::basic_io_scatter_t<typename outputstmtype::output_char_type> const *scatters,
	::std::size_t n)
{
	using char_type = typename outputstmtype::output_char_type;
	if (n == 0)
	{
		// An empty character scatter run emits no output.
		return;
	}
	if (n == 1)
	{
		// A single character scatter avoids scatter-write overhead and writes the range directly when non-empty.
		auto [base, len] = *scatters;
		if (len != 0)
		{
			// The single character scatter contains payload characters that can be written contiguously.
			::fast_io::operations::decay::write_all_decay(outstm, base, base + len);
		}
		return;
	}
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	if constexpr (threshold_chars != 0 &&
				  ::fast_io::details::decay::print_has_direct_write_operations<outputstmtype> &&
				  ::fast_io::details::decay::print_has_direct_scatter_write_operations<outputstmtype>)
	{
		// Eligible multi-scatter character runs first try bounded stack coalescing before scatter output.
		char_type buffer[threshold_chars];
		char_type *curr{buffer};
		::std::size_t remaining{threshold_chars - 1u};
		for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
		{
			::std::size_t const len{iter->len};
			if (remaining < len)
			{
				// The character run exceeds the coalescing threshold, so preserve scatter output directly.
				if (::fast_io::details::decay::print_scatter_write_all_try_repack_small(outstm, scatters, n))
				{
					return;
				}
				::fast_io::operations::decay::scatter_write_all_decay(outstm, scatters, n);
				return;
			}
			if (len != 0)
			{
				// Non-empty character scatter payloads are appended to the contiguous coalescing buffer.
				curr = ::fast_io::details::decay::print_small_scatter_copy_n(iter->base, len, curr);
				remaining -= len;
			}
		}
		if (curr != buffer)
		{
			// At least one character payload was coalesced, so emit the compact contiguous character range.
			::fast_io::operations::decay::write_all_decay(outstm, buffer, curr);
		}
		return;
	}
	// Streams that cannot coalesce this path emit the original character scatter descriptors directly.
	if (::fast_io::details::decay::print_scatter_write_all_try_repack_small(outstm, scatters, n))
	{
		return;
	}
	::fast_io::operations::decay::scatter_write_all_decay(outstm, scatters, n);
}

/// @brief    Dispatches scatter output to the byte or character scatter writer.
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   scatter_type  the scatter descriptor type
/// @param    outstm        the output stream reference
/// @param    scatters      the first scatter descriptor
/// @param    n             the number of scatter descriptors
template <typename outputstmtype, typename scatter_type>
inline constexpr void print_scatter_write_all_dispatch(outputstmtype outstm, scatter_type const *scatters,
													   ::std::size_t n)
{
	if constexpr (::std::same_as<scatter_type, ::fast_io::io_scatter_t>)
	{
		// Byte scatter descriptors are routed to the byte-oriented coalescing writer.
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters, n);
	}
	else
	{
		// Character scatter descriptors are routed to the character-oriented coalescing writer.
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters, n);
	}
}

/// @brief    Appends a newline scatter when requested and writes the full scatter run.
/// @tparam   needprintlf   true when the final scatter slot is reserved for a newline
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   char_type     the output character type
/// @tparam   scatter_type  the scatter descriptor type
/// @param    outstm        the output stream reference
/// @param    scatters      the scatter descriptor storage
/// @param    n             the number of scatter descriptors, including the optional newline slot
template <bool needprintlf, typename outputstmtype, ::std::integral char_type, typename scatter_type>
inline constexpr void print_scatter_write_all_dispatch_with_line(outputstmtype outstm, scatter_type *scatters,
																 ::std::size_t n)
{
	if constexpr (needprintlf)
	{
		// The caller reserved the final scatter slot for the trailing newline output.
		if constexpr (::std::same_as<scatter_type, ::fast_io::io_scatter_t>)
		{
			// Byte scatter output stores the newline length in bytes.
			scatters[n - 1u] = ::fast_io::details::decay::line_scatter_common<char_type, void>;
		}
		else
		{
			// Character scatter output stores the newline length in characters.
			scatters[n - 1u] = ::fast_io::details::decay::line_scatter_common<char_type>;
		}
	}
	::fast_io::details::decay::print_scatter_write_all_dispatch(outstm, scatters, n);
}

/// @brief    Writes one character scatter followed by a newline.
/// @details  Streams with byte-write support receive byte scatter descriptors; other streams receive character
///           scatter descriptors. Descriptor storage is placed on the stack when permitted by the stack policy.
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   char_type     the output character type
/// @param    outstm        the output stream reference
/// @param    scatter_res   the scatter payload to write before the newline
template <typename outputstmtype, ::std::integral char_type>
inline constexpr void print_single_scatter_with_line(outputstmtype outstm,
													 ::fast_io::basic_io_scatter_t<char_type> scatter_res)
{
	if constexpr (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>)
	{
		// Byte-capable streams receive byte scatter descriptors so wide characters have correct byte lengths.
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<2u, ::fast_io::io_scatter_t>)
		{
			// Small byte scatter descriptor storage is kept on the stack.
			::fast_io::io_scatter_t scatters[2];
			scatters[0] = {scatter_res.base, scatter_res.len * sizeof(char_type)};
			scatters[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), sizeof(char_type)};
			::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters, 2u);
		}
		else
		{
			// Oversized byte scatter descriptor storage is allocated before writing the two-part output.
			::fast_io::details::local_operator_new_array_ptr<::fast_io::io_scatter_t> scatters(2u);
			scatters.ptr[0] = {scatter_res.base, scatter_res.len * sizeof(char_type)};
			scatters.ptr[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), sizeof(char_type)};
			::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters.ptr, 2u);
		}
	}
	else
	{
		// Character-only streams receive character scatter descriptors directly.
		using scatter_type = ::fast_io::basic_io_scatter_t<char_type>;
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<2u, scatter_type>)
		{
			// Small character scatter descriptor storage is kept on the stack.
			scatter_type scatters[2];
			scatters[0] = scatter_res;
			scatters[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), 1u};
			::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters, 2u);
		}
		else
		{
			// Oversized character scatter descriptor storage is allocated before writing the two-part output.
			::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(2u);
			scatters.ptr[0] = scatter_res;
			scatters.ptr[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), 1u};
			::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters.ptr, 2u);
		}
	}
}

/// @brief    Materializes reserve-scatters output into byte scatter descriptors and writes it.
/// @tparam   line          true when a trailing newline scatter is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   char_type     the output character type
/// @tparam   T             the reserve-scatters printable type
/// @param    outstm        the output stream reference
/// @param    scatters      the byte scatter descriptor storage
/// @param    t             the argument to materialize
template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_reserve_scatters_bytes_with_scatter(outputstmtype outstm, ::fast_io::io_scatter_t *scatters,
																T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type>)
	{
		// Small reserve storage for byte scatter output is materialized on the stack.
		char_type buffer[reserve_buffer_size];
		::fast_io::io_scatter_t *ptr{::fast_io::details::decay::prrsvsct_byte_common_impl(scatters, buffer, t)};
		if constexpr (line)
		{
			// The line variant appends a byte-length newline scatter after the materialized scatters.
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type, void>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		// Large reserve storage for byte scatter output is allocated before materialization.
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
		::fast_io::io_scatter_t *ptr{::fast_io::details::decay::prrsvsct_byte_common_impl(scatters, buffer.ptr, t)};
		if constexpr (line)
		{
			// The line variant appends a byte-length newline scatter after the materialized scatters.
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type, void>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

/// @brief    Materializes reserve-scatters output into character scatter descriptors and writes it.
/// @tparam   line          true when a trailing newline scatter is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   char_type     the output character type
/// @tparam   T             the reserve-scatters printable type
/// @param    outstm        the output stream reference
/// @param    scatters      the character scatter descriptor storage
/// @param    t             the argument to materialize
template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_reserve_scatters_basic_with_scatter(
	outputstmtype outstm, ::fast_io::basic_io_scatter_t<char_type> *scatters, T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type>)
	{
		// Small reserve storage for character scatter output is materialized on the stack.
		char_type buffer[reserve_buffer_size];
		auto ptr{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters, buffer, t)
					 .scatters_pos_ptr};
		if constexpr (line)
		{
			// The line variant appends a character-length newline scatter after the materialized scatters.
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		// Large reserve storage for character scatter output is allocated before materialization.
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
		auto ptr{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters, buffer.ptr,
											   t)
					 .scatters_pos_ptr};
		if constexpr (line)
		{
			// The line variant appends a character-length newline scatter after the materialized scatters.
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

/// @brief    Emits one reserve-scatters printable argument through the best scatter descriptor representation.
/// @details  Runtime paths prefer byte scatter descriptors when the stream supports byte writes; constant-evaluated
///           paths and character-only streams use typed character scatter descriptors.
/// @tparam   line          true when a trailing newline scatter is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   char_type     the output character type
/// @tparam   T             the reserve-scatters printable type
/// @param    outstm        the output stream reference
/// @param    t             the argument to emit
template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_control_single_reserve_scatters(outputstmtype outstm, T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	static_assert(sz.scatters_size != SIZE_MAX);
	constexpr ::std::size_t scattersnum{sz.scatters_size + static_cast<::std::size_t>(line)};
#if __cpp_if_consteval >= 202106L
	if !consteval
#else
	if (!__builtin_is_constant_evaluated())
#endif
	{
		// Runtime byte-capable streams can use byte scatter descriptors for reserve-scatters output.
		if constexpr (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<
						  outputstmtype>)
		{
			if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scattersnum,
																						  ::fast_io::io_scatter_t>)
			{
				// Small byte scatter descriptor storage is kept on the stack.
				::fast_io::io_scatter_t scatters[scattersnum];
				::fast_io::details::decay::print_reserve_scatters_bytes_with_scatter<line, outputstmtype, char_type>(
					outstm, scatters, t);
			}
			else
			{
				// Large byte scatter descriptor storage is allocated before materializing the output.
				::fast_io::details::local_operator_new_array_ptr<::fast_io::io_scatter_t> scatters(scattersnum);
				::fast_io::details::decay::print_reserve_scatters_bytes_with_scatter<line, outputstmtype, char_type>(
					outstm, scatters.ptr, t);
			}
			return;
		}
	}
	using scatter_type = ::fast_io::basic_io_scatter_t<char_type>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scattersnum, scatter_type>)
	{
		// Character scatter descriptor storage is kept on the stack when the policy allows it.
		scatter_type scatters[scattersnum];
		::fast_io::details::decay::print_reserve_scatters_basic_with_scatter<line, outputstmtype, char_type>(
			outstm, scatters, t);
	}
	else
	{
		// Character scatter descriptor storage is allocated when the fixed run exceeds the stack policy.
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scattersnum);
		::fast_io::details::decay::print_reserve_scatters_basic_with_scatter<line, outputstmtype, char_type>(
			outstm, scatters.ptr, t);
	}
}

/// @brief    Drains a context-printable object through a caller-provided context buffer.
/// @details  Each iteration asks the context producer for the next filled window and writes it to the stream; the line
///           variant appends the trailing newline only to the final completed window.
/// @tparam   line          true when a trailing newline is appended after the final context window
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   context_type  the context state type
/// @tparam   T             the context-printable argument type
/// @tparam   char_type     the output character type
/// @param    outstm        the output stream reference
/// @param    st            the context object that produces output windows
/// @param    t             the argument being printed
/// @param    buffer        the first character in the context buffer
/// @param    buffered      one past the reusable buffered range passed to the producer
template <bool line, typename outputstmtype, typename context_type, typename T, ::std::integral char_type>
inline constexpr void print_context_define_to_window(outputstmtype outstm, context_type &st, T t, char_type *buffer,
													 char_type *buffered)
{
	for (;;)
	{
		auto [resit, done] = st.print_context_define(t, buffer, buffered);
		if constexpr (line)
		{
			// The line variant may append the newline to the final context output window.
			if (done)
			{
				// The producer finished, so the newline is appended before this final window is written.
				*resit = ::fast_io::char_literal_v<u8'\n', char_type>;
				++resit;
			}
			::fast_io::operations::decay::write_all_decay(outstm, buffer, resit);
			if (done)
			{
				// The final line-terminated context window has been written.
				return;
			}
		}
		else
		{
			// The non-line variant writes each produced context window without adding characters.
			::fast_io::operations::decay::write_all_decay(outstm, buffer, resit);
			if (done)
			{
				// The final context window has been written.
				return;
			}
		}
	}
}

/// @brief    Emits one already-forwarded printable control argument to an output stream.
/// @details  The dispatcher selects the most specialized single-argument path available: scatter, static reserve,
///           dynamic reserve, reserve-scatters, context printing, or the ordinary print_define customization.
/// @tparam   line   true when a trailing newline is appended after this argument
/// @tparam   output the decayed output stream reference type
/// @tparam   T      the argument type
/// @param    outstm the output stream reference
/// @param    t      the argument to emit
template <bool line = false, typename output, typename T>
	requires(::std::is_trivially_copyable_v<output> && ::std::is_trivially_copyable_v<T>)
inline constexpr void print_control_single(output outstm, T t)
{
	using char_type = typename output::output_char_type;
	using value_type = ::std::remove_cvref_t<T>;
	constexpr bool asan_activated{::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate};
	constexpr auto lfch{char_literal_v<u8'\n', char_type>};
	if constexpr (scatter_printable<char_type, value_type>)
	{
		// Scatter-printable arguments already expose a contiguous output range.
#if 0
		basic_io_scatter_t<char_type> scatter;
		if constexpr(::std::same_as<T,basic_io_scatter_t<char_type>>)
		{
			scatter=t;
		}
		else
		{
			scatter=print_scatter_define(::fast_io::io_reserve_type<char_type,value_type>,t);
		}
		if constexpr(line)
		{
			if constexpr(contiguous_output_stream<output>)
			{
				auto curr=obuffer_curr(out);
				auto end=obuffer_end(out);
				::std::ptrdiff_t sz(end-curr-1);
				::std::size_t const len{scatter.len};
				if(sz<static_cast<::std::ptrdiff_t>(len))
					fast_terminate();
				curr=non_overlapped_copy_n(scatter.base,scatter.len,curr);
				*curr=lfch;
				++curr;
				obuffer_set_curr(outstm,curr);
			}
			else if constexpr(::fast_io::operations::decay::defines::has_obuffer_basic_operations<output>)
			{
				auto curr=obuffer_curr(out);
				auto end=obuffer_end(out);
				::std::size_t const len{scatter.len};
				::std::ptrdiff_t sz(end-curr-1);
				if(static_cast<::std::ptrdiff_t>(len)<sz)[[likely]]
				{
					curr=details::non_overlapped_copy_n(scatter.base,len,curr);
					*curr=lfch;
					++curr;
					obuffer_set_curr(outstm,curr);
				}
				else
				{
					write(outstm,scatter.base,scatter.base+scatter.len);
					put(outstm,lfch);
				}
			}
			else 
			{
				write(outstm,scatter.base,scatter.base+scatter.len);
				write(outstm,__builtin_addressof(lfch),
				__builtin_addressof(lfch)+1);
			}
		}
		else
		{
			write(outstm,scatter.base,scatter.base+scatter.len);
		}
#endif
		basic_io_scatter_t<char_type> scatter_res{print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, t)};

		if constexpr (line)
		{
			// The line variant writes the scatter range and a trailing newline as one scatter-oriented output.
			::fast_io::details::decay::print_single_scatter_with_line(outstm, scatter_res);
		}
		else
		{
			// The non-line variant writes the scatter range directly.
			::fast_io::operations::decay::write_all_decay(outstm, scatter_res.base, scatter_res.base + scatter_res.len);
		}
	}
	else if constexpr (reserve_printable<char_type, value_type>)
	{
		// Static reserve-printable arguments have a compile-time output size and can be materialized directly.
		constexpr ::std::size_t real_size{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};
		constexpr ::std::size_t size{real_size + static_cast<::std::size_t>(line)};
		static_assert(real_size < PTRDIFF_MAX);
#if 0
		if constexpr(contiguous_output_stream<output>)
		{
			auto bcurr{obuffer_curr(outstm)};
			auto bend{obuffer_end(outstm)};
			::std::size_t diff{static_cast<::std::size_t>(bend-bcurr)};
			if(diff<size)[[unlikely]]
				fast_terminate();
			auto it{print_reserve_define(::fast_io::io_reserve_type<char_type,value_type>,bcurr,t)};
			if constexpr(line)
			{
				*it=lfch;
				++it;
			}
			obuffer_set_curr(outstm,it);
		}
		else
#endif
		{
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output> &&
						  !asan_activated)
			{
				// Buffered streams first try to emit the static reserve output into the current put area.
				char_type *bcurr{obuffer_curr(outstm)};
				char_type *bend{obuffer_end(outstm)};
				::std::ptrdiff_t const diff(bend - bcurr);
				bool smaller{static_cast<::std::ptrdiff_t>(size) < diff};
				if constexpr (minimum_buffer_output_stream_require_size_impl<output, size>)
				{
					// Streams with a sufficient declared minimum buffer can flush once and then emit in place.
					if (!smaller) [[unlikely]]
					{
						// The current put area is short, so prepare the minimum-size buffer before emission.
						obuffer_minimum_size_flush_prepare_define(outstm);
						bcurr = obuffer_curr(outstm);
					}
					bcurr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, bcurr, t);
					if constexpr (line)
					{
						// The line variant appends the newline in the same output-buffer commit.
						*bcurr = lfch;
						++bcurr;
					}
					obuffer_set_curr(outstm, bcurr);
				}
				else
				{
					// Without a minimum-buffer guarantee, choose between direct put-area output and fallback storage.
					if (smaller) [[likely]]
					{
						// The current put area has enough room, so emit directly and commit the new cursor.
						bcurr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, bcurr, t);
						if constexpr (line)
						{
							// The line variant appends the newline in the same output-buffer commit.
							*bcurr = lfch;
							++bcurr;
						}
						obuffer_set_curr(outstm, bcurr);
					}
					else [[unlikely]]
					{
						// The current put area is too small, so materialize the output outside the stream buffer.
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<size, char_type>)
						{
							// A small static reserve output is materialized on the stack and written once.
							char_type buffer[size];
							char_type *i{
								print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
							if constexpr (line)
							{
								// The line variant appends the newline before the stack buffer is written.
								*i = lfch;
								++i;
							}
							::fast_io::operations::decay::write_all_decay(outstm, buffer, i);
						}
						else
						{
							// A large static reserve output is materialized in heap storage and written once.
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
							char_type *i{
								print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
							if constexpr (line)
							{
								// The line variant appends the newline before the heap buffer is written.
								*i = lfch;
								++i;
							}
							::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, i);
						}
					}
				}
			}
			else
			{
				// Without a usable output buffer, choose stack or heap materialization for the static reserve output.
				if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<size, char_type>)
				{
					// Without a usable output buffer, small static reserve output is materialized on the stack.
					char_type buffer[size];
					char_type *i{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
					if constexpr (line)
					{
						// The line variant appends the newline before the stack buffer is written.
						*i = lfch;
						++i;
					}
					::fast_io::operations::decay::write_all_decay(outstm, buffer, i);
				}
				else
				{
					// Without a usable output buffer, large static reserve output is materialized in heap storage.
					::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
					char_type *i{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
					if constexpr (line)
					{
						// The line variant appends the newline before the heap buffer is written.
						*i = lfch;
						++i;
					}
					::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, i);
				}
			}
		}
	}
	else if constexpr (dynamic_reserve_printable<char_type, value_type>)
	{
		// Dynamic reserve-printable arguments are measured at run time before choosing a materialization path.
		::std::size_t size{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
		constexpr ::std::size_t stack_buffer_size{
			::fast_io::details::decay::dynamic_print_reserve_static_stack_size<line, char_type, value_type>()};
		if constexpr (line)
		{
			// The line variant reserves one additional character and validates pointer-difference bounds.
			constexpr ::std::size_t mx{::std::numeric_limits<::std::ptrdiff_t>::max() - 1};
			if (size >= mx)
			{
				// The measured run cannot be represented safely with a trailing newline.
				fast_terminate();
			}
			++size;
		}
		else
		{
			// The non-line variant only needs to validate the measured dynamic output size.
			constexpr ::std::size_t mx{::std::numeric_limits<::std::ptrdiff_t>::max()};
			if (mx < size)
			{
				// The measured run cannot be represented safely by pointer differences.
				fast_terminate();
			}
		}
#if 0
		if constexpr(contiguous_output_stream<output>)
		{
			auto bcurr{obuffer_curr(outstm)};
			auto bend{obuffer_end(outstm)};
			auto it{print_reserve_define(::fast_io::io_reserve_type<char_type,value_type>,bcurr,t,size)};
			::std::size_t diff{static_cast<::std::size_t>(bend-bcurr)};
			if(diff<size)[[unlikely]]
				fast_terminate();
			if constexpr(line)
			{
				*it=lfch;
				++it;
			}
			obuffer_set_curr(outstm,it);
		}
		else
#endif
		{
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output> &&
						  !asan_activated)
			{
				// Buffered streams first try to emit the dynamic reserve output into the current put area.
				auto curr{obuffer_curr(outstm)};
				auto ed{obuffer_end(outstm)};
				::std::ptrdiff_t diff(ed - curr);
				bool smaller{static_cast<::std::ptrdiff_t>(size) < diff};
				if (smaller)
				{
					// The current put area has enough room, so emit directly and commit the new cursor.
					auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t)};
					if constexpr (line)
					{
						// The line variant appends the newline in the same output-buffer commit.
						*it = lfch;
						++it;
					}
					obuffer_set_curr(outstm, it);
				}
				else
#if __has_cpp_attribute(unlikely)
					[[unlikely]]
#endif
				{
					// The current put area is too small, so dynamic output falls back to temporary storage.
					if constexpr (stack_buffer_size != 0)
					{
						// A static stack hint allows small dynamic output to avoid heap allocation.
						if (size <= stack_buffer_size)
						{
							// The measured dynamic output fits in the bounded stack buffer.
							char_type stack_buffer[stack_buffer_size];
							auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>,
														 stack_buffer, t)};
							if constexpr (line)
							{
								// The line variant appends the newline before the stack buffer is written.
								*it = lfch;
								++it;
							}
							::fast_io::operations::decay::write_all_decay(outstm, stack_buffer, it);
						}
						else
						{
							// The measured dynamic output exceeds the stack hint and is materialized on the heap.
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
							auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>,
														 newptr.ptr, t)};
							if constexpr (line)
							{
								// The line variant appends the newline before the heap buffer is written.
								*it = lfch;
								++it;
							}
							::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
						}
					}
					else
					{
						// Without a usable static stack hint, dynamic output is materialized on the heap.
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr,
													 t)};
						if constexpr (line)
						{
							// The line variant appends the newline before the heap buffer is written.
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
					}
				}
			}
			else
			{
				// Without a usable output buffer, dynamic output chooses between static stack hint and heap storage.
				if constexpr (stack_buffer_size != 0)
				{
					// Without a usable output buffer, a static stack hint may still avoid heap allocation.
					if (size <= stack_buffer_size)
					{
						// The measured dynamic output fits in the bounded stack buffer.
						char_type buffer[stack_buffer_size];
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
						if constexpr (line)
						{
							// The line variant appends the newline before the stack buffer is written.
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, buffer, it);
					}
					else
					{
						// The measured dynamic output exceeds the stack hint and is materialized on the heap.
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
						if constexpr (line)
						{
							// The line variant appends the newline before the heap buffer is written.
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
					}
				}
				else
				{
					// Without a stack hint, dynamic output is materialized on the heap and written once.
					::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
					auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
					if constexpr (line)
					{
						// The line variant appends the newline before the heap buffer is written.
						*it = lfch;
						++it;
					}
					::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
				}
			}
		}
	}
	else if constexpr (reserve_scatters_printable<char_type, value_type>)
	{
		// Reserve-scatters printable arguments are emitted through the scatter descriptor materialization path.
		::fast_io::details::decay::print_control_single_reserve_scatters<line, output, char_type>(outstm, t);
	}
	else if constexpr (::fast_io::transcode_imaginary_printable<char_type, value_type>)
	{
		// Transcode-imaginary printables are recognized here, but this freestanding path has no active emitter yet.
		// todo?
	}
	else if constexpr (context_printable<char_type, value_type>)
	{
		// Context-printable arguments stream repeated context windows until their producer reports completion.
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, value_type>))>::type st;
		constexpr ::std::size_t reserved_size{
			::fast_io::details::decay::context_print_static_buffer_size_v<line, char_type, value_type>};
		constexpr ::std::ptrdiff_t reserved_size_no_line{
			static_cast<::std::ptrdiff_t>(reserved_size - static_cast<::std::size_t>(line))};
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output>)
		{
			// Buffered streams let the context producer fill the current put area directly when possible.
			for (;;)
			{
				auto bcurr{obuffer_curr(outstm)};
				auto bed{obuffer_end(outstm)};
				if (bed <= bcurr)
#if __has_cpp_attribute(unlikely)
					[[unlikely]]
#endif
				{
					// An exhausted put area must be refreshed or replaced by an explicit context window.
					if constexpr (minimum_buffer_output_stream_require_size_impl<output, reserved_size>)
					{
						// Streams with a sufficient minimum buffer can prepare a fresh window for context output.
						obuffer_minimum_size_flush_prepare_define(outstm);
						bcurr = obuffer_curr(outstm);
						bed = obuffer_end(outstm);
					}
					else
					{
						// Streams without a sufficient minimum buffer flush before deciding on a fallback window.
						::fast_io::operations::decay::output_stream_buffer_flush_decay(outstm);
						bcurr = obuffer_curr(outstm);
						bed = obuffer_end(outstm);
						if (bed - bcurr < reserved_size_no_line)
#if __has_cpp_attribute(unlikely)
							[[unlikely]]
#endif
						{
							// Even after flushing, the put area is too small for one context producer window.
							if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
											  reserved_size, char_type>)
							{
								// A too-small put area falls back to a stack context buffer when it fits the policy.
								char_type buffer[reserved_size];
								char_type *buffered{buffer + reserved_size_no_line};
								::fast_io::details::decay::print_context_define_to_window<line>(
									outstm, st, t, buffer, buffered);
							}
							else
							{
								// A too-small put area falls back to heap context storage for large windows.
								::fast_io::details::local_operator_new_array_ptr<char_type> newptr(reserved_size);
								char_type *buffer{newptr.ptr};
								char_type *buffered{buffer + reserved_size_no_line};
								::fast_io::details::decay::print_context_define_to_window<line>(
									outstm, st, t, buffer, buffered);
							}
							return;
						}
					}
				}
				else
				{
					// A non-empty put area is handed directly to the context producer.
					auto [resit, done] = st.print_context_define(t, bcurr, bed);
					obuffer_set_curr(outstm, resit);
					if (done)
#if __has_cpp_attribute(likely)
						[[likely]]
#endif
					{
						// The context producer finished this argument, so only optional line termination remains.
						if constexpr (line)
						{
							// The line variant writes the trailing newline after the final context window.
							::fast_io::operations::decay::char_put_decay(outstm, lfch);
						}
						return;
					}
				}
			}
		}
		else
		{
			// Unbuffered context output uses an explicit context window sized by the static context requirement.
			if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserved_size, char_type>)
			{
				// Unbuffered streams use a stack context buffer when the fixed window fits the policy.
				char_type buffer[reserved_size];
				char_type *buffered{buffer + reserved_size_no_line};
				::fast_io::details::decay::print_context_define_to_window<line>(outstm, st, t, buffer, buffered);
			}
			else
			{
				// Unbuffered streams allocate context storage when the fixed window exceeds the stack policy.
				::fast_io::details::local_operator_new_array_ptr<char_type> newptr(reserved_size);
				char_type *buffer{newptr.ptr};
				char_type *buffered{buffer + reserved_size_no_line};
				::fast_io::details::decay::print_context_define_to_window<line>(outstm, st, t, buffer, buffered);
			}
			return;
		}
	}
	else if constexpr (printable<char_type, value_type>)
	{
		// Generic printable arguments delegate to their print_define customization.
		print_define(::fast_io::io_reserve_type<char_type, value_type>, outstm, t);
		if constexpr (line)
		{
			// The line variant appends the newline after the customization has emitted its output.
			::fast_io::operations::decay::char_put_decay(outstm, lfch);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<value_type>, ::fast_io::io_null_t>)
	{
		// Null output intentionally emits nothing.
	}
	else
	{
		// Unrecognized argument types fail here with the printable concept diagnostic.
		constexpr bool no{printable<char_type, value_type>};
		static_assert(no, "type not printable");
	}
}

/// @brief    Describes a contiguous run that can share a context-capture buffer.
/// @details  The scan tracks context producers, dynamic reserve producers with bounded stack hints, and static
///           reserve bursts so the caller can size one reusable capture buffer for the run.
struct context_capture_run_result
{
	::std::size_t position{};
	::std::size_t context_buffer_size{};
	::std::size_t dynamic_buffer_size{};
	::std::size_t max_static_reserve_burst_size{};
	::std::size_t leading_static_reserve_burst_size{};
	bool has_context{};
	bool has_dynamic{};
};

/// @brief    Finds the leading run that benefits from context-capture buffering.
/// @details  Static reserve outputs are grouped into burst sizes, while context and bounded dynamic reserve outputs
///           contribute reusable window sizes. Null outputs keep position accounting without increasing storage.
/// @tparam   char_type the character type used by the print run
/// @tparam   Arg       the current argument type
/// @tparam   Args      the remaining argument types
/// @return   context_capture_run_result the capture-buffer requirements for the leading run
template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr context_capture_run_result find_context_capture_run_n()
{
	using nocvreft = ::std::remove_cvref_t<Arg>;
	if constexpr (::fast_io::reserve_printable<char_type, nocvreft>)
	{
		// Static reserve output can be accumulated into the leading contiguous reserve burst.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so this reserve output extends the front of the run.
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		static_assert(sz != 0);
		++ret.position;
		ret.leading_static_reserve_burst_size =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.leading_static_reserve_burst_size, sz);
		if (ret.max_static_reserve_burst_size < ret.leading_static_reserve_burst_size)
		{
			// The current static reserve burst is the largest contiguous reserve output seen so far.
			ret.max_static_reserve_burst_size = ret.leading_static_reserve_burst_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		// Bounded dynamic reserve output participates through its reusable maximum stack window.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before this dynamic producer resets the static burst.
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t static_stack_size{
			print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		constexpr ::std::size_t dynamic_buffer_size{
			::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<static_stack_size, char_type>()};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_dynamic = true;
		if (ret.dynamic_buffer_size < dynamic_buffer_size)
		{
			// The capture buffer must fit the largest bounded dynamic reserve window in the run.
			ret.dynamic_buffer_size = dynamic_buffer_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, nocvreft>)
	{
		// Context-printable output anchors the capture run and requires its declared context window.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before this context producer resets the static burst.
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t context_buffer_size{
			::fast_io::details::decay::context_print_static_buffer_size_v<false, char_type, nocvreft>};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_context = true;
		if (ret.context_buffer_size < context_buffer_size)
		{
			// The capture buffer must fit the largest static context window in the run.
			ret.context_buffer_size = context_buffer_size;
		}
		return ret;
	}
	else if constexpr (::std::same_as<nocvreft, ::fast_io::io_null_t>)
	{
		// Null output keeps the argument position in the run while requiring no capture storage.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so the null can extend the positional prefix.
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		++ret.position;
		return ret;
	}
	else
	{
		// Any other output protocol terminates the context-capture prefix.
		return {};
	}
}

/// @brief    Writes pending context-capture buffer contents and resets the cursor.
/// @tparam   output    the decayed output stream reference type
/// @tparam   char_type the character type stored in the capture buffer
/// @param    outstm    the output stream reference
/// @param    buffer    the first character in the capture buffer
/// @param    curr      the current output cursor, reset to buffer after flushing
template <typename output, ::std::integral char_type>
inline constexpr void context_capture_flush(output outstm, char_type *buffer, char_type *&curr)
{
	if (curr != buffer)
	{
		// Pending captured output is emitted as one contiguous range before the buffer is reused.
		::fast_io::operations::decay::write_all_decay(outstm, buffer, curr);
		curr = buffer;
	}
}

/// @brief    Emits a captured context/static-reserve run through a reusable buffer.
/// @details  Static reserve and bounded dynamic reserve values are appended to the capture buffer when they fit;
///           larger values fall back to their ordinary single-control output path. Context producers stream windows
///           through the same buffer and the final recursive step appends the optional newline.
/// @tparam   needprintlf true when the final captured output should append a newline
/// @tparam   n           the number of arguments remaining in the captured run
/// @tparam   output      the decayed output stream reference type
/// @tparam   char_type   the character type stored in the capture buffer
/// @tparam   T           the current argument type
/// @tparam   Args        the remaining argument types
/// @param    outstm      the output stream reference
/// @param    buffer      the first character in the capture buffer
/// @param    curr        the current output cursor
/// @param    end         one past the capture buffer
/// @param    t           the current argument to emit
/// @param    args        the remaining arguments to emit
template <bool needprintlf, ::std::size_t n, typename output, ::std::integral char_type, typename T,
		  typename... Args>
inline constexpr void print_context_capture_run(output outstm, char_type *buffer, char_type *curr, char_type *end,
												T t, Args... args)
{
	if constexpr (n != 0)
	{
		// A non-empty captured run consumes the current argument and then continues recursively.
		using value_type = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, value_type>)
		{
			// Static reserve output is appended to the capture buffer when the fixed size fits.
			constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};
			static_assert(sz < PTRDIFF_MAX);
			if (static_cast<::std::size_t>(end - curr) < sz)
			{
				// The current buffer tail is too small, so existing captured output is flushed first.
				::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
			}
			if (sz <= static_cast<::std::size_t>(end - buffer))
			{
				// The static reserve output fits in the reusable capture buffer.
				curr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t);
			}
			else
			{
				// The static reserve output is larger than the capture buffer and must use its normal path.
				::fast_io::details::decay::print_control_single<false>(outstm, t);
			}
		}
		else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, value_type>)
		{
			// Bounded dynamic reserve output is measured before deciding whether it fits the capture buffer.
			::std::size_t const sz{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
			if (static_cast<::std::size_t>(end - curr) < sz)
			{
				// The current buffer tail is too small, so existing captured output is flushed first.
				::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
			}
			if (sz <= static_cast<::std::size_t>(end - buffer))
			{
				// The measured dynamic reserve output fits in the reusable capture buffer.
				curr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t);
			}
			else
			{
				// The measured dynamic reserve output is too large and must use its normal path.
				::fast_io::details::decay::print_control_single<false>(outstm, t);
			}
		}
		else if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, value_type>)
		{
			// Context-printable output streams repeated producer windows through the capture buffer.
			typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, value_type>))>::type st;
			for (;;)
			{
				if (curr == end)
				{
					// A full capture buffer is emitted before asking the context producer for another window.
					::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
				}
				auto [resit, done] = st.print_context_define(t, curr, end);
				curr = resit;
				if (done)
				{
					// The context producer has completed the current argument.
					break;
				}
			}
		}
		else if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
		{
			// Null output intentionally contributes no captured characters.
		}

		if constexpr (n == 1)
		{
			// The final captured argument is responsible for the optional newline and final flush.
			if constexpr (needprintlf)
			{
				// The line variant appends a trailing newline into the capture buffer before flushing.
				if (curr == end)
				{
					// A full buffer is flushed to make room for the newline character.
					::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
				}
				*curr = ::fast_io::char_literal_v<u8'\n', char_type>;
				++curr;
			}
			::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
		}
		else
		{
			// More captured arguments remain, so continue the run with the updated buffer cursor.
			::fast_io::details::decay::print_context_capture_run<needprintlf, n - 1>(outstm, buffer, curr, end,
																					 args...);
		}
	}
}

#if 0
template<bool ln,::std::integral char_type,::std::size_t n,typename Arg,typename ...Args>
inline constexpr char_type* printrsvcontiguousimpl(char_type* iter,Arg arg,Args... args)
{
	if constexpr(sizeof...(Args)!=0)
	{
		ret = find_continuous_scatters_n<char_type,Args...>();
	}
	if constexpr(::fast_io::scatter_printable<char_type,Arg>)
	{
		return {ret.position+1,ret.neededspace,ret.hasdynamicreserve};
	}
	else if constexpr(::fast_io::reserve_printable<char_type,Arg>)
	{
		constexpr
			::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type,Arg>)};
		return {ret.position+1,
			::fast_io::details::add_overflow(ret.neededspace,sz),
			ret.hasdynamicreserve};
	}
	else if constexpr(::fast_io::dynamic_reserve_printable<char_type,Arg>)
	{
		return {ret.position+1,ret.neededspace,true};
	}
}
#endif

/// @brief    Emits a control sequence while preserving the caller's final line policy.
/// @details  The final argument receives the line flag when requested; earlier arguments are emitted without a
///           newline so the whole sequence behaves like one logical print run.
/// @tparam   ln     true when the final argument should append a newline
/// @tparam   output the decayed output stream reference type
/// @tparam   T      the current argument type
/// @tparam   Args   the remaining argument types
/// @param    outstm the output stream reference
/// @param    t      the current argument to emit
/// @param    args   the remaining arguments to emit
template <bool ln, typename output, typename T, typename... Args>
// Force-inline rationale: this is the small top-level control recursion for a known line flag; inlining lets the
// newline case and the final single-control tail fold into the selected output path instead of leaving a call chain.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_line(output outstm, T t, Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		// The final control argument receives the caller's line policy.
		print_control_single<ln>(outstm, t);
	}
	else
	{
#if (defined(__OPTIMIZE__) || defined(__OPTIMIZE_SIZE__)) && 0
		// Disabled experimental multi-control path would emit the run through a specialized recursion.
		print_controls_line_multi_impl<ln, 0>(outstm, t, args...);
#else
		if constexpr (ln)
		{
			// Line mode emits the head without a newline and recurses so only the tail receives the newline.
			print_control_single<false>(outstm, t);
			print_controls_line<ln>(outstm, args...);
		}
		else
		{
			// Non-line mode emits every control argument without adding line termination.
			print_control_single<false>(outstm, t);
			(print_control<false>(outstm, args), ...);
		}
#endif
	}
}

/// @brief    Materializes N reserve-printable arguments into an existing contiguous buffer.
/// @details  Null arguments are skipped, while reserve-printable arguments advance the cursor through
///           print_reserve_define. The returned cursor marks one past the materialized output.
/// @tparam   n         the number of argument positions to consume
/// @tparam   char_type the buffer character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @param    ptr       the current output cursor
/// @param    t         the current argument
/// @param    args      the remaining arguments
/// @return   char_type* one past the materialized reserve output
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
// Force-inline rationale: reserve-only emission is a pre-sized cursor advance; inlining exposes the fixed recursion
// depth so null arguments disappear and the cursor updates become straight-line stores.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *print_n_reserve(char_type *ptr, T t, Args... args)
{
	if constexpr (n == 0)
	{
		// No argument positions are requested, so the output cursor is unchanged.
		return ptr;
	}
	else
	{
		// At least one argument position remains in this contiguous reserve run.
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			// Null output consumes a position but emits no characters into the reserve buffer.
			if constexpr (sizeof...(Args) == 0 || n < 2)
			{
				// The null argument is the final consumed position.
				return ptr;
			}
			else
			{
				// Additional positions remain, so continue with the same output cursor.
				return print_n_reserve<n - 1>(ptr, args...);
			}
		}
		else
		{
			// Reserve-printable output materializes directly into the current contiguous buffer.
			ptr = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t);
			if constexpr (sizeof...(Args) == 0 || n < 2)
			{
				// The current reserve output completes the requested run.
				return ptr;
			}
			else
			{
				// More reserve positions remain, so continue from the advanced cursor.
				return print_n_reserve<n - 1>(ptr, args...);
			}
		}
	}
}

/// @brief    Builds N scatter descriptors from scatter-printable arguments.
/// @details  Null arguments are skipped, byte scatter descriptors convert character counts to byte counts, and typed
///           scatter descriptors preserve character counts for character-oriented output.
/// @tparam   n           the number of argument positions to consume
/// @tparam   char_type   the print character type
/// @tparam   scattertype void for byte scatters, or char_type for character scatters
/// @tparam   T           the current argument type
/// @tparam   Args        the remaining argument types
/// @param    pscatters   the current scatter descriptor cursor
/// @param    t           the current argument
/// @param    args        the remaining arguments
template <::std::size_t n, ::std::integral char_type, typename scattertype, typename T, typename... Args>
// Force-inline rationale: scatter descriptor construction is pure template dispatch; inlining keeps the descriptor
// count, byte/character scatter conversion, and null elision visible to the final scatter write.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_n_scatters(basic_io_scatter_t<scattertype> *pscatters,
#if __has_cpp_attribute(maybe_unused)
									   [[maybe_unused]]
#endif
									   T t,
#if __has_cpp_attribute(maybe_unused)
									   [[maybe_unused]]
#endif
									   Args... args)
{
	if constexpr (n != 0)
	{
		// A non-empty scatter run consumes the current argument position.
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			// Null output contributes no scatter descriptor but may be followed by more output positions.
			if constexpr (1 < n)
			{
				// Continue building descriptors for the remaining argument positions.
				return print_n_scatters<n - 1, char_type>(pscatters, args...);
			}
		}
		else if constexpr (::std::same_as<::std::remove_cvref_t<T>, basic_io_scatter_t<scattertype>>)
		{
			// Existing scatter descriptors can be copied or byte-length-adjusted without calling customization.
			if constexpr (::std::same_as<scattertype, void>)
			{
				// Byte scatter output stores the payload extent in bytes.
				*pscatters = io_scatter_t{t.base, t.len * sizeof(char_type)};
			}
			else
			{
				// Character scatter output preserves the existing typed descriptor.
				*pscatters = t;
			}
		}
		else
		{
			// Scatter-printable output is queried for its single contiguous output range.
			basic_io_scatter_t<char_type> sct{print_scatter_define(::fast_io::io_reserve_type<char_type, T>, t)};
			if constexpr (::std::same_as<scattertype, void>)
			{
				// Byte scatter output stores the produced scatter extent in bytes.
				*pscatters = io_scatter_t{sct.base, sct.len * sizeof(char_type)};
			}
			else
			{
				// Character scatter output stores the produced scatter extent in characters.
				*pscatters = sct;
			}
		}
		if constexpr (1 < n)
		{
			// More consumed positions remain, so advance the descriptor cursor and continue.
			++pscatters;
			return print_n_scatters<n - 1, char_type>(pscatters, args...);
		}
	}
}

/// @brief    Computes the total character count for N scatter-printable/null arguments.
/// @tparam   n         the number of argument positions to consume
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @param    t         the current argument
/// @param    args      the remaining arguments
/// @return   ::std::size_t the combined scatter character count
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_n_scatter_total_size(
#if __has_cpp_attribute(maybe_unused)
	[[maybe_unused]]
#endif
	T t,
#if __has_cpp_attribute(maybe_unused)
	[[maybe_unused]]
#endif
	Args... args)
{
	if constexpr (n == 0)
	{
		return 0;
	}
	else
	{
		using nocvreft = ::std::remove_cvref_t<T>;
		::std::size_t current{};
		if constexpr (!::std::same_as<nocvreft, ::fast_io::io_null_t>)
		{
			current = print_scatter_define(::fast_io::io_reserve_type<char_type, nocvreft>, t).len;
		}
		if constexpr (n == 1)
		{
			return current;
		}
		else
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				current, ::fast_io::details::decay::print_n_scatter_total_size<n - 1, char_type>(args...));
		}
	}
}

/// @brief    Copies N scatter-printable/null arguments into a contiguous buffer.
/// @tparam   n         the number of argument positions to consume
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @param    ptr       the current output cursor
/// @param    t         the current argument
/// @param    args      the remaining arguments
/// @return   char_type* one past the copied scatter output
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *print_n_scatter_materialize(char_type *ptr,
#if __has_cpp_attribute(maybe_unused)
														[[maybe_unused]]
#endif
														T t,
#if __has_cpp_attribute(maybe_unused)
														[[maybe_unused]]
#endif
														Args... args)
{
	if constexpr (n == 0)
	{
		return ptr;
	}
	else
	{
		using nocvreft = ::std::remove_cvref_t<T>;
		if constexpr (!::std::same_as<nocvreft, ::fast_io::io_null_t>)
		{
			auto scatter{print_scatter_define(::fast_io::io_reserve_type<char_type, nocvreft>, t)};
			ptr = ::fast_io::details::decay::print_small_scatter_copy_n(scatter.base, scatter.len, ptr);
		}
		if constexpr (n == 1)
		{
			return ptr;
		}
		else
		{
			return ::fast_io::details::decay::print_n_scatter_materialize<n - 1, char_type>(ptr, args...);
		}
	}
}

/// @brief    Tries to emit a scatter-only prefix as one contiguous output range.
/// @tparam   needprintlf   true when the final emitted output should append a newline
/// @tparam   position      the number of argument positions included in the scatter prefix
/// @tparam   char_type     the print character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
/// @return   bool true when the prefix was emitted without building scatter descriptors
template <bool needprintlf, ::std::size_t position, ::std::integral char_type, typename outputstmtype, typename T,
		  typename... Args>
inline constexpr bool print_controls_scatters_try_materialize(outputstmtype optstm, T t, Args... args)
{
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_full_output_coalesce_threshold<char_type, outputstmtype>()};
	if constexpr (threshold_chars != 0 &&
				  ::fast_io::details::decay::print_has_direct_write_operations<outputstmtype> &&
				  ::fast_io::details::decay::print_stack_buffer_size_within_limit<threshold_chars, char_type>)
	{
		::std::size_t const total_size{::fast_io::details::intrinsics::add_or_overflow_die(
			::fast_io::details::decay::print_n_scatter_total_size<position, char_type>(t, args...),
			static_cast<::std::size_t>(needprintlf))};
		if (total_size == 0)
		{
			return true;
		}
		if (total_size <= threshold_chars)
		{
			char_type buffer[threshold_chars];
			char_type *ptr{
				::fast_io::details::decay::print_n_scatter_materialize<position, char_type>(buffer, t, args...)};
			if constexpr (needprintlf)
			{
				*ptr = ::fast_io::char_literal_v<u8'\n', char_type>;
				++ptr;
			}
			::fast_io::operations::decay::write_all_decay(optstm, buffer, ptr);
			return true;
		}
	}
	return false;
}

/// @brief    Computes the combined run-time reserve size for dynamic reserve arguments in a prefix.
/// @tparam   n         the number of argument positions to inspect
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @param    t         the current argument
/// @param    args      the remaining arguments
/// @return   ::std::size_t the sum of dynamic reserve sizes in the inspected prefix
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
// Force-inline rationale: dynamic reserve sizing feeds a single allocation decision; inlining folds non-dynamic
// arguments to zero and keeps overflow-checked additions adjacent to the caller's allocation branch.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t ndynamic_print_reserve_size(T t, Args... args)
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		// An empty prefix has no dynamic reserve size contribution.
		return 0;
	}
	else if constexpr (n == 1)
	{
		// The final inspected position either contributes its measured dynamic size or zero.
		if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			// Dynamic reserve output contributes its run-time measured size.
			return print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>, t);
		}
		else
		{
			// Non-dynamic output contributes no run-time reserve size.
			return 0;
		}
	}
	else
	{
		// Multiple positions are accumulated with overflow checking.
		if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			// The current dynamic reserve size is added to the remaining prefix size.
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>, t),
				::fast_io::details::decay::ndynamic_print_reserve_size<n - 1, char_type>(args...));
		}
		else
		{
			// Non-dynamic output is skipped while the remaining prefix is measured.
			return ::fast_io::details::decay::ndynamic_print_reserve_size<n - 1, char_type>(args...);
		}
	}
}

/// @brief    Computes the combined static stack hints for dynamic reserve arguments in a prefix.
/// @tparam   n         the number of argument positions to inspect
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @return   ::std::size_t the sum of declared static stack hints in the inspected prefix
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
// Force-inline rationale: static stack-budget aggregation decides whether the dynamic reserve path can stay on the
// stack; inlining lets the all-static/heap-fallback branch collapse before code generation.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t ndynamic_print_reserve_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		// An empty prefix needs no dynamic reserve stack budget.
		return 0;
	}
	else if constexpr (n == 1)
	{
		// The final inspected position either contributes its static stack hint or zero.
		if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
		{
			// A bounded dynamic reserve producer contributes its declared static stack hint.
			return print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>);
		}
		else
		{
			// Producers without a static hint contribute no stack budget.
			return 0;
		}
	}
	else
	{
		// Multiple positions are accumulated with overflow checking.
		if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
		{
			// The current static stack hint is added to the remaining prefix budget.
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>),
				::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<n - 1, char_type, Args...>());
		}
		else
		{
			// Output without a static dynamic-reserve hint is skipped while the remaining prefix is inspected.
			return ::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<n - 1, char_type, Args...>();
		}
	}
}

/// @brief    Tests whether every dynamic reserve argument in a prefix has a static stack hint.
/// @tparam   n         the number of argument positions to inspect
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @return   bool true when stack-buffer materialization can be bounded statically
template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
inline constexpr bool ndynamic_print_reserve_has_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		// An empty prefix trivially satisfies the static-stack-hint requirement.
		return true;
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft> &&
					   !::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		// A dynamic reserve producer without a static hint forces heap materialization.
		return false;
	}
	else if constexpr (n == 1)
	{
		// The final inspected position did not violate the static-stack-hint requirement.
		return true;
	}
	else
	{
		// Continue checking the remaining prefix positions.
		return ::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<n - 1, char_type, Args...>();
	}
}

/// @brief    Builds mixed reserve/scatter descriptors for a prefix into caller-provided storage.
/// @tparam   needprintlf true when the final descriptor should include a trailing newline
/// @tparam   n           the number of argument positions to consume
/// @tparam   char_type   the print character type
/// @tparam   scattertype void for byte scatters, or char_type for character scatters
/// @tparam   T           the current argument type
/// @tparam   Args        the remaining argument types
/// @param    pscatters   the current scatter descriptor cursor
/// @param    ptr         the current reserve-buffer cursor
/// @param    t           the current argument
/// @param    args        the remaining arguments
/// @return   one past the final scatter descriptor written
template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
// Force-inline rationale: the forward declaration carries the same attribute as the definitions so compilers that
// bind force-inline on the first declaration do not outline the reserve/scatter cursor state machine.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve(basic_io_scatter_t<scattertype> *pscatters, char_type *ptr, T t,
											   Args... args);

/// @brief    Tests whether the next argument in a pack is reserve-like.
/// @tparam   char_type the print character type
/// @tparam   T         the next argument type
/// @tparam   Args      ignored remaining argument types
/// @return   bool true when the next argument can continue a reserve coalescing run
template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool print_next_is_reserve() noexcept
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::reserve_printable<char_type, nocvreft> ||
				  ::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
	{
		// Static and dynamic reserve-printable arguments can extend the current reserve scatter.
		return true;
	}
	else
	{
		// Non-reserve output terminates the current reserve scatter.
		return false;
	}
}

/// @brief    Empty-pack overload for reserve-lookahead checks.
/// @tparam   char_type the print character type
/// @return   bool false because no next argument exists
template <::std::integral char_type>
inline constexpr bool print_next_is_reserve() noexcept
{
	return false;
}

/// @brief    Continues coalescing adjacent reserve outputs into the current scatter descriptor.
/// @details  The continuation keeps one reserve-buffer base pointer while reserve-like arguments remain adjacent.
///           When the run ends, it commits the coalesced range as either a byte or character scatter descriptor.
/// @tparam   needprintlf true when the final consumed output should append a newline
/// @tparam   n           the number of argument positions remaining
/// @tparam   char_type   the print character type
/// @tparam   scattertype void for byte scatters, or char_type for character scatters
/// @tparam   T           the current argument type
/// @tparam   Args        the remaining argument types
/// @param    pscatters   the current scatter descriptor cursor
/// @param    base        the first character in the coalesced reserve range
/// @param    ptr         the current reserve-buffer cursor
/// @param    t           the current argument
/// @param    args        the remaining arguments
/// @return   one past the final scatter descriptor written
template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
// Force-inline rationale: this continuation coalesces adjacent reserve fragments into one scatter entry; inlining
// keeps the base pointer, current pointer, and next-argument classification in one foldable cursor pipeline.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve_cont(basic_io_scatter_t<scattertype> *pscatters, char_type *base,
													char_type *ptr, T t, Args... args)
{
	if constexpr (n != 0)
	{
		// A non-empty continuation consumes the current position in the active reserve run.
		using nocvreft = ::std::remove_cvref_t<T>;
		if constexpr (reserve_printable<char_type, nocvreft> || dynamic_reserve_printable<char_type, nocvreft>)
		{
			// Reserve-like output extends the active contiguous reserve buffer range.
			ptr = print_reserve_define(::fast_io::io_reserve_type<char_type, nocvreft>, ptr, t);
			if constexpr (::fast_io::details::decay::print_next_is_reserve<char_type, Args...>())
			{
				// The next output is also reserve-like, so keep coalescing into the same scatter descriptor.
				return ::fast_io::details::decay::print_n_scatters_reserve_cont<needprintlf, n - 1, char_type>(
					pscatters, base, ptr, args...);
			}
			else
			{
				// The reserve run ends here, so commit the buffered range as one scatter descriptor.
				if constexpr (n == 1 && needprintlf)
				{
					// The line variant appends its newline inside the final coalesced reserve range.
					*ptr = char_literal_v<u8'\n', char_type>;
					++ptr;
				}
				::std::size_t const sz{static_cast<::std::size_t>(ptr - base)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					// Byte scatter output records the coalesced reserve range length in bytes.
					*pscatters = io_scatter_t{base, sz * sizeof(char_type)};
				}
				else
				{
					// Character scatter output records the coalesced reserve range length in characters.
					*pscatters = basic_io_scatter_t<char_type>{base, sz};
				}
				++pscatters;
				if constexpr (1 < n)
				{
					// More output remains after the reserve run, so resume the mixed materializer.
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pscatters, ptr, args...);
				}
			}
		}
		else
		{
			// A non-reserve argument terminates the current reserve run before it is processed.
			::std::size_t const sz{static_cast<::std::size_t>(ptr - base)};
			if constexpr (::std::same_as<scattertype, void>)
			{
				// Byte scatter output commits the completed reserve run in bytes.
				*pscatters = io_scatter_t{base, sz * sizeof(char_type)};
			}
			else
			{
				// Character scatter output commits the completed reserve run in characters.
				*pscatters = basic_io_scatter_t<char_type>{base, sz};
			}
			++pscatters;
			if constexpr (1 < n)
			{
				// The terminating argument still belongs to the overall run and is processed next.
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
					pscatters, ptr, t, args...);
			}
		}
	}
	// No more output positions remain, so return the current scatter cursor.
	return pscatters;
}

/// @brief    Materializes a mixed scatter/reserve prefix into scatter descriptors and reserve storage.
/// @details  Reserve-like outputs are coalesced into contiguous reserve scatters, existing scatter outputs are
///           forwarded as descriptors, reserve-scatters outputs append their own descriptors, and nulls are skipped.
/// @tparam   needprintlf true when the final consumed output should append a newline
/// @tparam   n           the number of argument positions to consume
/// @tparam   char_type   the print character type
/// @tparam   scattertype void for byte scatters, or char_type for character scatters
/// @tparam   T           the current argument type
/// @tparam   Args        the remaining argument types
/// @param    pscatters   the current scatter descriptor cursor
/// @param    ptr         the current reserve-buffer cursor
/// @param    t           the current argument
/// @param    args        the remaining arguments
/// @return   one past the final scatter descriptor written
template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
// Force-inline rationale: the mixed reserve/scatter materializer is the boundary where compile-time argument classes
// become descriptor writes; inlining avoids outlining the recursive state that determines the final scatter count.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve(basic_io_scatter_t<scattertype> *pscatters, char_type *ptr, T t,
											   Args... args)
{
	if constexpr (n != 0)
	{
		// A non-empty mixed run consumes the current argument position.
		using nocvreft = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, nocvreft> ||
					  ::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			// Reserve-like output is materialized into reserve storage before becoming a scatter descriptor.
			auto ptred{print_reserve_define(::fast_io::io_reserve_type<char_type, nocvreft>, ptr, t)};
			if constexpr (sizeof...(Args) != 0 &&
						  ::fast_io::details::decay::print_next_is_reserve<char_type, Args...>())
			{
				// Adjacent reserve-like output is coalesced into the same scatter descriptor.
				return ::fast_io::details::decay::print_n_scatters_reserve_cont<needprintlf, n - 1, char_type>(
					pscatters, ptr, ptred, args...);
			}
			else
			{
				// This reserve output stands alone and is committed as one scatter descriptor.
				if constexpr (n == 1 && needprintlf)
				{
					// The line variant appends its newline to the final reserve range before committing it.
					*ptred = char_literal_v<u8'\n', char_type>;
					++ptred;
				}
				::std::size_t const sz{static_cast<::std::size_t>(ptred - ptr)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					// Byte scatter output records the reserve range length in bytes.
					*pscatters = io_scatter_t{ptr, sz * sizeof(char_type)};
				}
				else
				{
					// Character scatter output records the reserve range length in characters.
					*pscatters = basic_io_scatter_t<char_type>{ptr, sz};
				}
				++pscatters;
				if constexpr (1 < n)
				{
					// Remaining positions continue after the committed reserve range.
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pscatters, ptred, args...);
				}
			}
		}
		else if constexpr (::fast_io::scatter_printable<char_type, nocvreft>)
		{
			// Scatter-printable output contributes a descriptor without consuming reserve storage.
			if constexpr (::std::same_as<nocvreft, basic_io_scatter_t<scattertype>>)
			{
				// Existing scatter descriptor arguments are copied directly when their representation matches.
				if constexpr (::std::same_as<scattertype, void>)
				{
					// Byte scatter output converts an existing character length to bytes.
					*pscatters = io_scatter_t{t.base, t.len * sizeof(char_type)};
				}
				else
				{
					// Character scatter output preserves the existing typed descriptor.
					*pscatters = t;
				}
			}
			else
			{
				// General scatter-printable output is queried for its exposed output range.
				basic_io_scatter_t<char_type> sct{print_scatter_define(::fast_io::io_reserve_type<char_type, T>, t)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					// Byte scatter output stores the exposed range length in bytes.
					*pscatters = io_scatter_t{sct.base, sct.len * sizeof(char_type)};
				}
				else
				{
					// Character scatter output stores the exposed range length in characters.
					*pscatters = sct;
				}
			}
			++pscatters;
			if constexpr (n == 1 && needprintlf)
			{
				// The line variant appends a separate newline scatter after the final scatter output.
				*pscatters = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
				++pscatters;
			}
			if constexpr (1 < n)
			{
				// Remaining positions continue with the same reserve-buffer cursor.
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(pscatters,
																										  ptr, args...);
			}
		}
		else if constexpr (::fast_io::reserve_scatters_printable<char_type, nocvreft>)
		{
			// Reserve-scatters output supplies one or more descriptors and advances reserve storage itself.
			if constexpr (::std::same_as<scattertype, void>)
			{
				// Byte scatter output uses the byte-oriented reserve-scatters adapter.
				auto pit{::fast_io::details::decay::prrsvsct_byte_common_rsvsc_impl(pscatters, ptr, t)};
				if constexpr (1 < n)
				{
					// Remaining positions continue from the adapter's updated cursors.
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pit.scatters_pos_ptr, pit.reserve_pos_ptr, args...);
				}
				else if constexpr (n == 1 && needprintlf)
				{
					// The line variant appends a newline scatter after the final byte reserve-scatters output.
					*pit.scatters_pos_ptr = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
					++pit.scatters_pos_ptr;
				}
				return pit.scatters_pos_ptr;
			}
			else
			{
				// Character scatter output uses the typed reserve-scatters customization directly.
				auto pit{
					print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, nocvreft>, pscatters, ptr, t)};
				if constexpr (1 < n)
				{
					// Remaining positions continue from the customization's updated cursors.
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pit.scatters_pos_ptr, pit.reserve_pos_ptr, args...);
				}
				else if constexpr (n == 1 && needprintlf)
				{
					// The line variant appends a newline scatter after the final typed reserve-scatters output.
					*pit.scatters_pos_ptr = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
					++pit.scatters_pos_ptr;
				}
				return pit.scatters_pos_ptr;
			}
		}
		else if constexpr (::std::same_as<nocvreft, ::fast_io::io_null_t>)
		{
			// Null output consumes a position while emitting no scatter descriptor.
			if constexpr (n == 1 && needprintlf)
			{
				// A final null still carries the caller's requested trailing newline.
				*pscatters = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
				++pscatters;
			}
			if constexpr (1 < n)
			{
				// Continue with the next argument while preserving the current cursors.
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(pscatters,
																										  ptr, args...);
			}
		}
	}
	// No more positions remain, so return the current scatter cursor.
	return pscatters;
}

/// @brief    Writes a scatter-only prefix using stack or heap descriptor storage.
/// @tparam   needprintlf   true when the final descriptor should be a trailing newline
/// @tparam   position      the number of argument positions included in the scatter prefix
/// @tparam   scatterscount the number of scatter descriptors to write
/// @tparam   char_type     the print character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::integral char_type,
		  typename outputstmtype, typename T, typename... Args>
inline constexpr void print_controls_scatters(outputstmtype optstm, T t, Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if (::fast_io::details::decay::print_controls_scatters_try_materialize<needprintlf, position, char_type>(
			optstm, t, args...))
	{
		return;
	}
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		// Small scatter descriptor runs are built on the stack and written once.
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_n_scatters<position, char_type>(scatters, t, args...);
		::fast_io::details::decay::print_scatter_write_all_dispatch_with_line<needprintlf, outputstmtype, char_type>(
			optstm, scatters, scatterscount);
	}
	else
	{
		// Large scatter descriptor runs allocate descriptor storage before one scatter write.
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_n_scatters<position, char_type>(scatters.ptr, t, args...);
		::fast_io::details::decay::print_scatter_write_all_dispatch_with_line<needprintlf, outputstmtype, char_type>(
			optstm, scatters.ptr, scatterscount);
	}
}

/// @brief    Materializes a mixed reserve/scatter prefix with caller-provided scatter descriptor storage.
/// @tparam   needprintlf   true when the final consumed output should append a newline
/// @tparam   position      the number of argument positions included in the prefix
/// @tparam   mxsize        the static reserve-buffer character requirement
/// @tparam   char_type     the print character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   scatter_type  the scatter descriptor representation
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    scatters      the caller-provided scatter descriptor storage
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool needprintlf, ::std::size_t position, ::std::size_t mxsize, ::std::integral char_type,
		  typename outputstmtype, typename scatter_type, typename T, typename... Args>
inline constexpr void print_controls_scatters_reserve_with_scatter(outputstmtype optstm, scatter_type *scatters, T t,
																   Args... args)
{
	constexpr ::std::size_t buffer_size{mxsize == 0 ? 1u : mxsize};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffer_size, char_type>)
	{
		// Small reserve storage is materialized on the stack before scatter output is written.
		char_type buffer[buffer_size];
		auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																									   buffer, t,
																									   args...)};
		::fast_io::details::decay::print_scatter_write_all_dispatch(
			optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		// Large reserve storage is allocated before scatter descriptors are materialized and written.
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(buffer_size);
		auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																									   buffer.ptr, t,
																									   args...)};
		::fast_io::details::decay::print_scatter_write_all_dispatch(
			optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

/// @brief    Writes a mixed static reserve/scatter prefix using stack or heap descriptor storage.
/// @tparam   needprintlf   true when the final consumed output should append a newline
/// @tparam   position      the number of argument positions included in the prefix
/// @tparam   scatterscount the number of scatter descriptors available
/// @tparam   mxsize        the static reserve-buffer character requirement
/// @tparam   char_type     the print character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::size_t mxsize,
		  ::std::integral char_type, typename outputstmtype, typename T, typename... Args>
inline constexpr void print_controls_scatters_reserve(outputstmtype optstm, T t, Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		// Small scatter descriptor storage is kept on the stack for the mixed reserve/scatter run.
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_controls_scatters_reserve_with_scatter<needprintlf, position, mxsize,
																				char_type>(optstm, scatters, t, args...);
	}
	else
	{
		// Large scatter descriptor storage is allocated before the mixed run is materialized.
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_controls_scatters_reserve_with_scatter<needprintlf, position, mxsize,
																				char_type>(optstm, scatters.ptr, t,
																						   args...);
	}
}

/// @brief    Materializes a mixed dynamic reserve/scatter prefix with caller-provided descriptor storage.
/// @details  When every dynamic producer has a static stack hint and the measured total fits the bounded stack buffer,
///           reserve storage stays on the stack; otherwise the measured total drives one heap allocation.
/// @tparam   needprintlf           true when the final consumed output should append a newline
/// @tparam   position              the number of argument positions included in the prefix
/// @tparam   stack_buffer_size     the bounded stack reserve-buffer size
/// @tparam   has_static_stack_size true when all dynamic producers have static stack hints
/// @tparam   char_type             the print character type
/// @tparam   outputstmtype         the decayed output stream reference type
/// @tparam   scatter_type          the scatter descriptor representation
/// @tparam   T                     the current argument type
/// @tparam   Args                  the remaining argument types
/// @param    optstm                the output stream reference
/// @param    scatters              the caller-provided scatter descriptor storage
/// @param    totalsz               the measured reserve-buffer character requirement
/// @param    t                     the current argument
/// @param    args                  the remaining arguments
template <bool needprintlf, ::std::size_t position, ::std::size_t stack_buffer_size, bool has_static_stack_size,
		  ::std::integral char_type, typename outputstmtype, typename scatter_type, typename T, typename... Args>
// Force-inline rationale: the dynamic reserve writer has a hot stack-buffer fast path; inlining lets the caller's
// known stack budget and total-size branch select stack storage or heap storage without an extra outlined frame.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve_with_scatter(outputstmtype optstm, scatter_type *scatters,
																		   ::std::size_t totalsz, T t, Args... args)
{
	if constexpr (has_static_stack_size &&
				  ::fast_io::details::decay::print_stack_buffer_size_within_limit<stack_buffer_size, char_type>)
	{
		// A statically bounded dynamic run may use stack reserve storage when the measured total fits.
		if (totalsz <= stack_buffer_size)
		{
			// The measured dynamic reserve output fits in the bounded stack buffer and writes once.
			char_type buffer[stack_buffer_size];
			auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																										   buffer, t,
																										   args...)};
			::fast_io::details::decay::print_scatter_write_all_dispatch(
				optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
			return;
		}
	}
	// The measured reserve output needs heap storage before descriptors can be written.
	::fast_io::details::local_operator_new_array_ptr<char_type> buffer(totalsz == 0 ? 1u : totalsz);
	auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters, buffer.ptr,
																								   t, args...)};
	::fast_io::details::decay::print_scatter_write_all_dispatch(
		optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
}

/// @brief    Writes a mixed dynamic reserve/scatter prefix using stack or heap descriptor storage.
/// @tparam   needprintlf           true when the final consumed output should append a newline
/// @tparam   position              the number of argument positions included in the prefix
/// @tparam   scatterscount         the number of scatter descriptors available
/// @tparam   stack_buffer_size     the bounded stack reserve-buffer size
/// @tparam   has_static_stack_size true when all dynamic producers have static stack hints
/// @tparam   char_type             the print character type
/// @tparam   outputstmtype         the decayed output stream reference type
/// @tparam   T                     the current argument type
/// @tparam   Args                  the remaining argument types
/// @param    optstm                the output stream reference
/// @param    totalsz               the measured reserve-buffer character requirement
/// @param    t                     the current argument
/// @param    args                  the remaining arguments
template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::size_t stack_buffer_size,
		  bool has_static_stack_size, ::std::integral char_type, typename outputstmtype, typename T, typename... Args>
// Force-inline rationale: this wrapper chooses stack versus heap scatter storage from template constants; inlining
// keeps that storage decision adjacent to the dynamic reserve writer and prevents a redundant allocator-shaped call.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve(outputstmtype optstm, ::std::size_t totalsz, T t,
															  Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		// Small scatter descriptor storage is kept on the stack for the dynamic mixed run.
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_controls_dynamic_scatters_reserve_with_scatter<
			needprintlf, position, stack_buffer_size, has_static_stack_size, char_type>(optstm, scatters, totalsz, t,
																						args...);
	}
	else
	{
		// Large scatter descriptor storage is allocated before the dynamic mixed run is materialized.
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_controls_dynamic_scatters_reserve_with_scatter<
			needprintlf, position, stack_buffer_size, has_static_stack_size, char_type>(optstm, scatters.ptr, totalsz, t,
																						args...);
	}
}

/// @brief    Primary non-buffered control dispatcher for a freestanding print run.
/// @tparam   line          true when the final emitted output should append a newline
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   skippings     number of already-consumed head arguments to skip
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool line, typename outputstmtype, ::std::size_t skippings, typename T, typename... Args>
inline constexpr void print_controls_impl(outputstmtype optstm, T t, Args... args);

/// @brief    Empty-pack overload for the dynamic reserve/scatter fast-entry test.
/// @tparam   char_type the print character type
/// @return   bool false because no argument prefix exists
template <::std::integral char_type>
inline constexpr bool print_controls_dynamic_scatters_reserve_fast_entry_available() noexcept
{
	return false;
}

/// @brief    Tests whether a print run can enter the dynamic reserve/scatter fast path.
/// @tparam   char_type the print character type
/// @tparam   T         the current argument type
/// @tparam   Args      the remaining argument types
/// @return   bool true when the leading run contains both scatter output and dynamic reserve output
template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool print_controls_dynamic_scatters_reserve_fast_entry_available() noexcept
{
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	return res.position != 0 && res.hasscatters && res.hasdynamicreserve;
}

/// @brief    Emits the leading dynamic reserve/scatter run through the specialized fast path.
/// @tparam   line          true when the final emitted output should append a newline
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool line, typename outputstmtype, typename T, typename... Args>
// Force-inline rationale: the fast entry is reached only after compile-time contiguous-scatter analysis succeeds;
// inlining preserves those constants so the selected reserve/scatter path is emitted directly at the semantic boundary.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve_fast_entry(outputstmtype optstm, T t, Args... args)
{
	using char_type = typename outputstmtype::output_char_type;
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	static_assert(res.position != 0);
	static_assert(res.hasscatters && res.hasdynamicreserve);
	if constexpr (line)
	{
		// A line-terminated fast path must have a finite scatter count for the optional newline descriptor.
		static_assert(res.neededscatters != SIZE_MAX);
	}
	static_assert(SIZE_MAX != sizeof...(Args));
	constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
	constexpr bool needprintlf{n == res.position && line};
	constexpr ::std::size_t mxsize{
		static_cast<::std::size_t>(res.neededspace + static_cast<::std::size_t>(needprintlf))};
	constexpr ::std::size_t scatterscount{res.neededscatters +
										  static_cast<::std::size_t>(line && res.position == n)};
	constexpr bool has_static_stack_size{
		::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position, char_type, T,
																				Args...>()};
	constexpr ::std::size_t producer_static_stack_size{
		::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type, T, Args...>()};
	constexpr ::std::size_t dynamic_stack_budget{
		::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size, char_type>()};
	constexpr ::std::size_t stack_buffer_size{
		::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
	::std::size_t dynsz{::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
	::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
	::fast_io::details::decay::print_controls_dynamic_scatters_reserve<
		needprintlf, res.position, scatterscount, stack_buffer_size, has_static_stack_size, char_type>(optstm, totalsz,
																									   t, args...);
	if constexpr (res.position != n)
	{
		// Remaining arguments after the fast prefix continue through the general control dispatcher.
		print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
	}
}

/// @brief    Emits a freestanding control run without relying on the stream put area.
/// @details  The dispatcher coalesces context-capable runs, scatter-only prefixes, reserve-only prefixes, and mixed
///           reserve/scatter prefixes before recursively emitting any remaining arguments.
/// @tparam   line          true when the final emitted output should append a newline
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   skippings     number of already-consumed head arguments to skip
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool line, typename outputstmtype, ::std::size_t skippings = 0, typename T, typename... Args>
inline constexpr void print_controls_impl(outputstmtype optstm, T t, Args... args)
{
	using char_type = typename outputstmtype::output_char_type;
	using scatter_type [[maybe_unused]] = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		io_scatter_t, basic_io_scatter_t<char_type>>;
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	constexpr context_capture_run_result context_capture_res{
		::fast_io::details::decay::find_context_capture_run_n<char_type, T, Args...>()};
	if constexpr (skippings != 0)
	{
		// Already-emitted prefix positions are skipped before dispatching the remaining arguments.
		print_controls_impl<line, outputstmtype, skippings - 1>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		// A single remaining argument uses the specialized one-control emitter with the caller's line policy.
		print_control_single<line>(optstm, t);
	}
	else if constexpr (context_capture_res.has_context)
	{
		// Runs containing context-printable output use a reusable capture buffer to reduce small writes.
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr bool needprintlf{n == context_capture_res.position && line};
		/*
		Context and dynamic producer sizes are reusable streaming windows.
		Only consecutive static reserve producers contribute a burst size, so
		the final capture buffer uses max semantics instead of summing producer
		windows across the run.
		*/
		constexpr ::std::size_t context_dynamic_size{
			context_capture_res.context_buffer_size < context_capture_res.dynamic_buffer_size
				? context_capture_res.dynamic_buffer_size
				: context_capture_res.context_buffer_size};
		constexpr ::std::size_t reserve_context_dynamic_size{
			context_dynamic_size < context_capture_res.max_static_reserve_burst_size
				? context_capture_res.max_static_reserve_burst_size
				: context_dynamic_size};
		constexpr ::std::size_t buffer_size{reserve_context_dynamic_size < 32u ? 32u
																			   : reserve_context_dynamic_size};
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffer_size, char_type>)
		{
			// Small context-capture buffers stay on the stack while the captured run is emitted.
			char_type buffer[buffer_size];
			::fast_io::details::decay::print_context_capture_run<needprintlf, context_capture_res.position>(
				optstm, buffer, buffer, buffer + buffer_size, t, args...);
		}
		else
		{
			// Large context-capture buffers are allocated before the captured run is emitted.
			::fast_io::details::local_operator_new_array_ptr<char_type> newptr(buffer_size);
			char_type *buffer{newptr.ptr};
			::fast_io::details::decay::print_context_capture_run<needprintlf, context_capture_res.position>(
				optstm, buffer, buffer, buffer + buffer_size, t, args...);
		}
		if constexpr (context_capture_res.position != n)
		{
			// Arguments after the context-captured prefix continue through the general dispatcher.
			print_controls_impl<line, outputstmtype, context_capture_res.position - 1>(optstm, args...);
		}
	}
	else if constexpr (res.position == 0)
	{
		// The current argument cannot join a contiguous optimized prefix, so emit it alone.
		print_control_single<false>(optstm, t);
		print_controls_impl<line>(optstm, args...);
	}
	else
	{
		// A contiguous scatter/reserve-friendly prefix was found and can be emitted as one optimized run.
		if constexpr (line)
		{
			// A line-terminated optimized prefix must have a finite scatter count when a newline descriptor is needed.
			static_assert(res.neededscatters != SIZE_MAX);
		}
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr bool needprintlf{n == res.position && line};
		if constexpr (res.hasscatters && !res.hasreserve && !res.hasdynamicreserve)
		{
			// Scatter-only prefixes are emitted directly as scatter descriptors.
			constexpr ::std::size_t scatterscount{res.neededscatters + static_cast<::std::size_t>(needprintlf)};
			::fast_io::details::decay::print_controls_scatters<needprintlf, res.position, scatterscount, char_type>(
				optstm, t, args...);
			if constexpr (res.position != n)
			{
				// Arguments after the scatter-only prefix continue through the general dispatcher.
				print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
			}
		}
		else
		{
			// Prefixes with reserve output need temporary character storage before they can be written.
			constexpr ::std::size_t mxsize{
				static_cast<::std::size_t>(res.neededspace + static_cast<::std::size_t>(needprintlf))};
			if constexpr (!res.hasscatters)
			{
				// Reserve-only prefixes can be materialized as one contiguous output range.
				static_assert(!needprintlf || res.neededspace != SIZE_MAX);
				if constexpr (res.hasdynamicreserve)
				{
					// Dynamic reserve-only prefixes are measured before choosing stack or heap storage.
					constexpr bool has_static_stack_size{
						::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position,
																								char_type, T,
																								Args...>()};
					constexpr ::std::size_t producer_static_stack_size{
						::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type,
																							T, Args...>()};
					constexpr ::std::size_t dynamic_stack_budget{
						::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size,
																							 char_type>()};
					constexpr ::std::size_t stack_buffer_size{
						::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
					::std::size_t dynsz{
						::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
					::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
					if constexpr (has_static_stack_size &&
								  ::fast_io::details::decay::print_stack_buffer_size_within_limit<stack_buffer_size,
																								  char_type>)
					{
						// A statically bounded dynamic reserve prefix may stay on the stack.
						if (totalsz <= stack_buffer_size)
						{
							// The measured dynamic reserve prefix fits in the bounded stack buffer.
							char_type buffer[stack_buffer_size];
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t,
																									args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the contiguous stack buffer.
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
						else
						{
							// The measured dynamic reserve prefix exceeds the stack budget and uses heap storage.
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(totalsz);
							char_type *buffer{newptr.ptr};
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t,
																									args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the contiguous heap buffer.
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
					}
					else
					{
						// Dynamic reserve prefixes without bounded stack eligibility use heap storage.
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(totalsz);
						char_type *buffer{newptr.ptr};
						char_type *ptred{
							::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
						if constexpr (needprintlf)
						{
							// The line variant appends the newline to the contiguous heap buffer.
							*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
							++ptred;
						}
						::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
					}
				}
				else if constexpr (res.hasreserve)
				{
					// Static reserve-only prefixes have a compile-time storage requirement.
					if constexpr (res.neededspace == 0)
					{
						// A zero-sized reserve prefix can only emit the optional trailing newline.
						if constexpr (needprintlf)
						{
							// The line variant emits the trailing newline as a single character output.
							::fast_io::operations::decay::char_put_decay(optstm,
																		 ::fast_io::char_literal_v<u8'\n', char_type>);
						}
					}
					else
					{
						// Non-empty static reserve prefixes are materialized before a single contiguous write.
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<mxsize,
																									  char_type>)
						{
							// Small static reserve prefixes are materialized on the stack.
							char_type buffer[mxsize];
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the stack buffer.
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
						else
						{
							// Large static reserve prefixes are materialized in heap storage.
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(mxsize);
							char_type *buffer{newptr.ptr};
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the heap buffer.
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
					}
				}
			}
			else if constexpr (res.hasreserve && !res.hasdynamicreserve)
			{
				// Mixed scatter/static-reserve prefixes are emitted through prebuilt scatter descriptors.
				constexpr ::std::size_t scatterscount{res.neededscatters +
													  static_cast<::std::size_t>(line && res.position == n)};
				::fast_io::details::decay::print_controls_scatters_reserve<needprintlf, res.position, scatterscount,
																		   mxsize, char_type>(optstm, t, args...);
			}
			else if constexpr (res.hasdynamicreserve)
			{
				// Mixed scatter/dynamic-reserve prefixes are measured before descriptor materialization.
				constexpr ::std::size_t scatterscount{res.neededscatters +
													  static_cast<::std::size_t>(line && res.position == n)};
				constexpr bool has_static_stack_size{
					::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position, char_type, T,
																							Args...>()};
				constexpr ::std::size_t producer_static_stack_size{
					::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type, T,
																						Args...>()};
				constexpr ::std::size_t dynamic_stack_budget{
					::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size,
																						 char_type>()};
				constexpr ::std::size_t stack_buffer_size{
					::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
				::std::size_t dynsz{
					::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
				::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
				::fast_io::details::decay::print_controls_dynamic_scatters_reserve<
					needprintlf, res.position, scatterscount, stack_buffer_size, has_static_stack_size, char_type>(
					optstm, totalsz, t, args...);
			}
			if constexpr (res.position != n)
			{
				// Arguments after the optimized prefix continue through the general dispatcher.
				print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
			}
		}
	}
}

/// @brief    Emits a freestanding control run while taking advantage of an output stream put area.
/// @details  Scatter prefixes bypass the put area through scatter writes; reserve prefixes first try the current
///           output buffer and only fall back to temporary storage when the put area is too small.
/// @tparam   line          true when the final emitted output should append a newline
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   skippings     number of already-consumed head arguments to skip
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
/// @param    optstm        the output stream reference
/// @param    t             the current argument
/// @param    args          the remaining arguments
template <bool line, typename outputstmtype, ::std::size_t skippings = 0, typename T, typename... Args>
inline constexpr void print_controls_buffer_impl(outputstmtype optstm, T t, Args... args)
{
	if constexpr (skippings != 0)
	{
		// Already-emitted prefix positions are skipped before the buffered dispatcher resumes.
		::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype, skippings - 1>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		// A single remaining argument uses the one-control emitter with the caller's line policy.
		print_control_single<line>(optstm, t);
	}
	else
	{
		// Multiple arguments may expose scatter or reserve prefixes that can be grouped before recursion.
		using char_type = typename outputstmtype::output_char_type;
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr auto scatters_result{
			::fast_io::details::decay::find_continuous_scatters_reserve_n<true, char_type, T, Args...>()};
		using scatter_type [[maybe_unused]] = ::std::conditional_t<
			::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
			io_scatter_t, basic_io_scatter_t<char_type>>;
		if constexpr (scatters_result.position != 0)
		{
			// A leading scatter-friendly prefix is emitted through scatter descriptors.
			if constexpr (line)
			{
				// A line-terminated scatter prefix must have a finite position for the optional newline descriptor.
				static_assert(scatters_result.position != SIZE_MAX);
			}

			constexpr bool needprintlf{n == scatters_result.position && line};
			constexpr ::std::size_t scatterscount{scatters_result.position - scatters_result.null +
												  static_cast<::std::size_t>(needprintlf)};
			::fast_io::details::decay::print_controls_scatters<needprintlf, scatters_result.position,
															   scatterscount, char_type>(optstm, t, args...);
			if constexpr (scatters_result.position != n)
			{
				// Arguments after the scatter prefix continue through the buffered dispatcher.
				::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype,
																	  scatters_result.position - 1>(optstm, args...);
			}
		}
		else
		{
			// Without a scatter prefix, try to group a reserve-only prefix into the output buffer.
			constexpr auto rsvresult{
				::fast_io::details::decay::find_continuous_scatters_reserve_n<false, char_type, T, Args...>()};
			if constexpr (1 < rsvresult.position)
			{
				// Reserve prefixes with more than one position can amortize buffer checks and writes.
				constexpr bool needprintlf{n == rsvresult.position && line};
				constexpr ::std::size_t buffersize{rsvresult.neededspace + static_cast<::std::size_t>(needprintlf)};
				char_type *bcurr{obuffer_curr(optstm)};
				char_type *bend{obuffer_end(optstm)};
				::std::ptrdiff_t const diff(bend - bcurr);
				bool smaller{static_cast<::std::ptrdiff_t>(buffersize) < diff};
				if constexpr (minimum_buffer_output_stream_require_size_impl<outputstmtype, buffersize>)
				{
					// Streams with a sufficient minimum put area can prepare the buffer once before writing in place.
					if (!smaller) [[unlikely]]
					{
						// The current put area is short, so prepare a minimum-size buffer for the reserve prefix.
						obuffer_minimum_size_flush_prepare_define(optstm);
						bcurr = obuffer_curr(optstm);
					}
					bcurr =
						::fast_io::details::decay::print_n_reserve<rsvresult.position, char_type>(bcurr, t, args...);
					if constexpr (needprintlf)
					{
						// The line variant appends the newline in the same output-buffer commit.
						*bcurr = ::fast_io::char_literal_v<u8'\n', char_type>;
						++bcurr;
					}
					obuffer_set_curr(optstm, bcurr);
				}
				else
				{
					// Streams without a minimum-size guarantee use the current put area only when it already fits.
					if (smaller) [[likely]]
					{
						// The current put area can hold the entire reserve prefix.
						bcurr =
							::fast_io::details::decay::print_n_reserve<rsvresult.position, char_type>(bcurr, t,
																									  args...);
						if constexpr (needprintlf)
						{
							// The line variant appends the newline in the same output-buffer commit.
							*bcurr = ::fast_io::char_literal_v<u8'\n', char_type>;
							++bcurr;
						}
						obuffer_set_curr(optstm, bcurr);
					}
					else [[unlikely]]
					{
						// The current put area is too small, so materialize the reserve prefix out of line.
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffersize,
																									  char_type>)
						{
							// Small reserve prefixes fall back to stack materialization and one write.
							char_type buffer[buffersize];
							char_type *ptr{::fast_io::details::decay::print_n_reserve<rsvresult.position,
																					  char_type>(buffer, t,
																								 args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the stack buffer before writing.
								*ptr = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptr;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptr);
						}
						else
						{
							// Large reserve prefixes fall back to heap materialization and one write.
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(buffersize);
							char_type *buffer{newptr.ptr};
							char_type *ptr{::fast_io::details::decay::print_n_reserve<rsvresult.position,
																					  char_type>(buffer, t,
																								 args...)};
							if constexpr (needprintlf)
							{
								// The line variant appends the newline to the heap buffer before writing.
								*ptr = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptr;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptr);
						}
					}
				}
				if constexpr (rsvresult.position != n)
				{
					// Arguments after the reserve prefix continue through the buffered dispatcher.
					::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype, rsvresult.position - 1>(
						optstm, args...);
				}
			}
			else
			{
				// No useful prefix was found, so emit the current argument and continue with the tail.
				::fast_io::details::decay::print_control_single<line && sizeof...(args) == 0>(optstm, t);
				if constexpr (sizeof...(args) != 0)
				{
					// The tail still owns the caller's final line policy.
					::fast_io::details::decay::print_controls_buffer_impl<line>(optstm, args...);
				}
			}
		}
	}
}

/// @brief    Returns the semantic node stored inside a parameter object, or the value itself.
/// @tparam   T the semantic candidate type
/// @param    t the semantic candidate object
/// @return   decltype(auto) the unwrapped semantic node reference or the original object
template <typename T>
inline constexpr decltype(auto) print_semantic_node_ref(T &&t) noexcept
{
	if constexpr (::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>>)
	{
		// Parameter objects forward semantic operations to their wrapped reference member.
		return ::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t).reference);
	}
	else
	{
		// Plain semantic nodes and leaves are already the reference used by the printer.
		return ::std::forward<T>(t);
	}
}

/// @brief    Detects semantic arguments that expand into print packs.
/// @tparam   T the argument type to inspect
template <typename T>
inline constexpr bool print_semantic_pack_argument_v =
	::fast_io::details::print_pack<T> ||
	(::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>> &&
	 ::fast_io::details::print_pack<
		 decltype(::fast_io::details::decay::print_semantic_node_ref(::std::declval<T>()))>);

/// @brief    Alias result type produced by io_print_alias for one semantic input argument.
/// @tparam   char_type the print character type
/// @tparam   T         the input argument type
template <::std::integral char_type, typename T>
using print_semantic_input_alias_t = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief    Forwarded semantic input type after aliasing and character-aware print forwarding.
/// @tparam   char_type the print character type
/// @tparam   T         the input argument type
template <::std::integral char_type, typename T>
using print_semantic_input_forward_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

/// @brief    Detects inputs that become semantic nodes after aliasing and print forwarding.
/// @tparam   char_type the print character type
/// @tparam   T         the input argument type
template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_input_argument_v =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<
		::std::remove_cvref_t<::fast_io::details::decay::print_semantic_input_alias_t<char_type, T>>> ||
	::fast_io::details::decay::print_semantic_node<
		::fast_io::details::decay::print_semantic_input_forward_t<char_type, T>>;

/// @brief    Applies the semantic input forwarding protocol to one argument.
/// @tparam   char_type the print character type
/// @tparam   T         the input argument type
/// @param    t         the input argument
/// @return   decltype(auto) the semantic node or forwarded print argument
template <::std::integral char_type, typename T>
inline constexpr decltype(auto) print_semantic_input_forward(T &&t) noexcept
{
	if constexpr (::fast_io::details::decay::print_semantic_node_no_parameter_v<
					  ::std::remove_cvref_t<
						  decltype(::fast_io::io_print_alias(::std::forward<T>(t)))>>)
	{
		// Alias results that are already parameterless semantic nodes must not be forwarded again.
		return ::fast_io::io_print_alias(::std::forward<T>(t));
	}
	else
	{
		// Ordinary alias results use the character-aware print forwarding path.
		return ::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::forward<T>(t)));
	}
}

/// @brief    Invokes a continuation with every element from a semantic pack.
/// @tparam   T            the pack-like semantic node type
/// @tparam   continuation the continuation receiving expanded elements
/// @tparam   I            the pack indices to expand
/// @param    t            the pack-like semantic node
/// @param    cont         the continuation to invoke
/// @return   decltype(auto) the continuation result
template <typename T, typename continuation, ::std::size_t... I>
inline constexpr decltype(auto) print_semantic_pack_apply_impl(T &&t, continuation &&cont,
															   ::std::index_sequence<I...>)
{
	auto &&pack_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	return cont(::fast_io::containers::get<I>(::std::forward<decltype(pack_ref)>(pack_ref).storage)...);
}

/// @brief    Expands a semantic pack using its declared pack size.
/// @tparam   T            the pack-like semantic node type
/// @tparam   continuation the continuation receiving expanded elements
/// @param    t            the pack-like semantic node
/// @param    cont         the continuation to invoke
/// @return   decltype(auto) the continuation result
template <typename T, typename continuation>
inline constexpr decltype(auto) print_semantic_pack_apply(T &&t, continuation &&cont)
{
	using pack_type =
		::std::remove_cvref_t<decltype(::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t)))>;
	return ::fast_io::details::decay::print_semantic_pack_apply_impl(
		::std::forward<T>(t), ::std::forward<continuation>(cont), ::std::make_index_sequence<pack_type::size>{});
}

/// @brief    Prefix continuation that prepends a stored value reference to later semantic arguments.
/// @tparam   continuation the downstream continuation type
/// @tparam   T            the stored value type
template <typename continuation, typename T>
struct print_semantic_value_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	/// @brief    Invokes the downstream continuation with the stored value before the tail arguments.
	/// @tparam   TailArgs the tail argument types
	/// @param    tail_args the tail arguments
	/// @return   decltype(auto) the downstream continuation result
	template <typename... TailArgs>
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<T>(*valueptr), ::std::forward<TailArgs>(tail_args)...);
	}
};

/// @brief    Prefix continuation that prepends a stored lvalue to later semantic arguments.
/// @tparam   continuation the downstream continuation type
/// @tparam   T            the stored lvalue type
template <typename continuation, typename T>
struct print_semantic_lvalue_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	T *valueptr;

	/// @brief    Invokes the downstream continuation with the stored lvalue before the tail arguments.
	/// @tparam   TailArgs the tail argument types
	/// @param    tail_args the tail arguments
	/// @return   decltype(auto) the downstream continuation result
	template <typename... TailArgs>
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(*valueptr, ::std::forward<TailArgs>(tail_args)...);
	}
};

template <typename... Args>
[[nodiscard]] inline constexpr ::fast_io::containers::tuple<Args...>
print_semantic_pack_pointer_tuple(Args... args) noexcept
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
	return ::fast_io::containers::tuple<Args...>{args...};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

/// @brief    Completes semantic pack expansion when no input arguments remain.
/// @tparam   already_forwarded true when non-pack inputs were already print-forwarded
/// @tparam   char_type         the print character type
/// @tparam   continuation      the continuation receiving the expanded run
/// @param    cont              the continuation to invoke
/// @return   decltype(auto) the continuation result
template <bool already_forwarded, ::std::integral char_type, typename continuation>
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont)
{
	return cont();
}

/// @brief    Expands semantic pack arguments before invoking a continuation.
/// @tparam   already_forwarded true when non-pack inputs were already print-forwarded
/// @tparam   char_type         the print character type
/// @tparam   continuation      the continuation receiving the expanded run
/// @tparam   T                 the current argument type
/// @tparam   Args              the remaining argument types
/// @param    cont              the continuation to invoke
/// @param    t                 the current argument
/// @param    args              the remaining arguments
/// @return   decltype(auto) the continuation result
template <bool already_forwarded, ::std::integral char_type, typename continuation, typename T, typename... Args>
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont, T &&t, Args &&...args);

/// @brief    Reattaches already-expanded pack elements in front of the remaining tail.
/// @tparam   already_forwarded true when non-pack inputs were already print-forwarded
/// @tparam   char_type         the print character type
/// @tparam   continuation      the final continuation type
/// @tparam   ExpandedPackArgs  the expanded pack element types
template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... ExpandedPackArgs>
struct print_semantic_pack_expand_tail_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<ExpandedPackArgs> *...> expandedptrs;

	/// @brief    Invokes the final continuation with expanded pack elements followed by tail arguments.
	/// @tparam   I        the expanded pack indices
	/// @tparam   TailArgs the tail argument types
	/// @param    <unnamed> the index sequence selecting expanded pack element pointers
	/// @param    tail_args the tail arguments
	/// @return   decltype(auto) the final continuation result
	template <::std::size_t... I, typename... TailArgs>
	inline constexpr decltype(auto) operator_impl(::std::index_sequence<I...>, TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<ExpandedPackArgs>(*::fast_io::containers::get<I>(expandedptrs))...,
						  ::std::forward<TailArgs>(tail_args)...);
	}

	/// @brief    Builds the index sequence used to reattach expanded pack elements.
	/// @tparam   TailArgs the tail argument types
	/// @param    tail_args the tail arguments
	/// @return   decltype(auto) the final continuation result
	template <typename... TailArgs>
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return operator_impl(::std::make_index_sequence<sizeof...(ExpandedPackArgs)>{},
							 ::std::forward<TailArgs>(tail_args)...);
	}
};

/// @brief    Continues pack expansion after one pack has been expanded in the middle of a run.
/// @tparam   already_forwarded true when non-pack inputs were already print-forwarded
/// @tparam   char_type         the print character type
/// @tparam   continuation      the final continuation type
/// @tparam   Args              the original tail argument types
template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... Args>
struct print_semantic_pack_expand_middle_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<Args> *...> const *argptrs;

	/// @brief    Restarts pack expansion for the saved tail arguments.
	/// @tparam   tail_continuation the continuation carrying the already-expanded pack prefix
	/// @tparam   I                 the saved tail indices
	/// @param    tail_cont         the continuation receiving the expanded tail
	/// @param    <unnamed>         the index sequence selecting saved tail pointers
	/// @return   decltype(auto) the final continuation result
	template <typename tail_continuation, ::std::size_t... I>
	inline constexpr decltype(auto) operator_impl(tail_continuation &&tail_cont, ::std::index_sequence<I...>) const
	{
		return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::std::forward<tail_continuation>(tail_cont),
			::std::forward<Args>(*::fast_io::containers::get<I>(*argptrs))...);
	}

	/// @brief    Captures the expanded pack elements and resumes expansion for the saved tail.
	/// @tparam   ExpandedPackArgs the expanded pack element types
	/// @param    expanded_pack_args the expanded pack elements
	/// @return   decltype(auto) the final continuation result
	template <typename... ExpandedPackArgs>
	inline constexpr decltype(auto) operator()(ExpandedPackArgs &&...expanded_pack_args) const
	{
		return operator_impl(
			::fast_io::details::decay::print_semantic_pack_expand_tail_continuation<
				already_forwarded, char_type, continuation, ExpandedPackArgs...>{
				contptr,
				::fast_io::details::decay::print_semantic_pack_pointer_tuple(
					__builtin_addressof(expanded_pack_args)...)},
			::std::make_index_sequence<sizeof...(Args)>{});
	}
};

/// @brief    Entry continuation used when the current argument expands into a semantic pack.
/// @tparam   already_forwarded true when non-pack inputs were already print-forwarded
/// @tparam   char_type         the print character type
/// @tparam   continuation      the final continuation type
/// @tparam   Args              the saved tail argument types
template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... Args>
struct print_semantic_pack_expand_initial_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<Args> *...> argptrs;

	/// @brief    Receives the current pack's elements and resumes expansion with the saved tail.
	/// @tparam   PackArgs the expanded current-pack element types
	/// @param    pack_args the expanded current-pack elements
	/// @return   decltype(auto) the final continuation result
	template <typename... PackArgs>
	inline constexpr decltype(auto) operator()(PackArgs &&...pack_args) const
	{
		return ::fast_io::details::decay::print_semantic_pack_expand<false, char_type>(
			::fast_io::details::decay::print_semantic_pack_expand_middle_continuation<
				already_forwarded, char_type, continuation, Args...>{contptr, __builtin_addressof(argptrs)},
			::std::forward<PackArgs>(pack_args)...);
	}
};

/// @brief    Expands semantic packs and forwards ordinary semantic inputs before invoking a continuation.
/// @details  Pack arguments are replaced by their stored elements. Ordinary arguments either keep their already
///           forwarded form or are aliased and print-forwarded into a stable local value.
/// @tparam   already_forwarded true when ordinary inputs have already passed semantic forwarding
/// @tparam   char_type         the print character type
/// @tparam   continuation      the continuation receiving the expanded run
/// @tparam   T                 the current argument type
/// @tparam   Args              the remaining argument types
/// @param    cont              the continuation to invoke
/// @param    t                 the current argument
/// @param    args              the remaining arguments
/// @return   decltype(auto) the continuation result
template <bool already_forwarded, ::std::integral char_type, typename continuation, typename T, typename... Args>
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::print_pack<T> ||
				  (::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>> &&
				   ::fast_io::details::print_pack<
					   decltype(::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t)))>))
	{
		// Pack arguments are expanded in place and then rejoined with the saved tail arguments.
		return ::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<T>(t),
			::fast_io::details::decay::print_semantic_pack_expand_initial_continuation<
				already_forwarded, char_type, continuation, Args...>{__builtin_addressof(cont),
																	 ::fast_io::details::decay::print_semantic_pack_pointer_tuple(
																		 __builtin_addressof(args)...)});
	}
	else if constexpr (already_forwarded)
	{
		// Already-forwarded ordinary arguments are captured by reference and prepended to the expanded tail.
		return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::fast_io::details::decay::print_semantic_value_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
	else
	{
		// Ordinary inputs are aliased and print-forwarded once, then kept alive through the prefix continuation.
		auto forwarded{::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::forward<T>(t)))};
		return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::fast_io::details::decay::print_semantic_lvalue_prefix_continuation<
				continuation, decltype(forwarded)>{__builtin_addressof(cont), __builtin_addressof(forwarded)},
			::std::forward<Args>(args)...);
	}
}

/// @brief    Completes null filtering for a semantic argument run.
/// @details  If the original run contained only nulls, one null is preserved so the downstream printer sees a valid
///           no-output argument; otherwise all null arguments are removed.
/// @tparam   had_null     true when at least one null argument was seen
/// @tparam   has_value    true when at least one non-null argument was preserved
/// @tparam   continuation the continuation receiving the filtered run
/// @param    cont         the continuation to invoke
/// @return   decltype(auto) the continuation result
template <bool had_null, bool has_value, typename continuation>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_semantic_filter_nulls(continuation &&cont)
{
	if constexpr (!has_value && had_null)
	{
		// A run made only of nulls preserves one null to represent intentional empty output.
		return cont(::fast_io::io_null);
	}
	else
	{
		// A run with real values drops all nulls before invoking the continuation.
		return cont();
	}
}

/// @brief    Removes null arguments from a semantic run before flattened emission.
/// @tparam   had_null     true when at least one null argument was seen before T
/// @tparam   has_value    true when at least one non-null argument was preserved before T
/// @tparam   continuation the continuation receiving the filtered run
/// @tparam   T            the current argument type
/// @tparam   Args         the remaining argument types
/// @param    cont         the continuation to invoke
/// @param    t            the current argument
/// @param    args         the remaining arguments
/// @return   decltype(auto) the continuation result
template <bool had_null, bool has_value, typename continuation, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_semantic_filter_nulls(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
	{
		// Null output is removed from mixed semantic runs but remembered in case the whole run is null.
		return ::fast_io::details::decay::print_semantic_filter_nulls<true, has_value>(
			::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
	}
	else
	{
		// Non-null output is preserved by prepending it to the filtered tail continuation.
		return ::fast_io::details::decay::print_semantic_filter_nulls<had_null, true>(
			::fast_io::details::decay::print_semantic_value_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

/// @brief    Checks whether one decayed print argument is accepted by the freestanding dispatcher.
/// @tparam   char_type the output character type
/// @tparam   T         the decayed argument type
template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single
	: ::std::bool_constant<
		  ::fast_io::printable<char_type, T> || ::fast_io::reserve_printable<char_type, T> ||
		  ::fast_io::dynamic_reserve_printable<char_type, T> || ::fast_io::scatter_printable<char_type, T> ||
		  ::fast_io::reserve_scatters_printable<char_type, T> || ::fast_io::context_printable<char_type, T> ||
		  ::fast_io::transcode_imaginary_printable<char_type, T> ||
		  ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
		  print_semantic_params_okay<char_type, ::std::remove_cvref_t<T>>::value>
{};

} // namespace details::decay

namespace operations
{

namespace decay
{

/// @brief    Freestanding print dispatcher for runs that do not require semantic pack expansion.
/// @details  The dispatcher handles status-print customizations, empty line output, mutex-wrapped streams, buffered
///           streams, and the generic control emitter.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   Args          the argument types in the print run
/// @param    optstm        the output stream reference
/// @param    args          the arguments to emit
/// @return   decltype(auto) the selected output path's return value, when any
template <bool line, typename outputstmtype, typename... Args>
inline constexpr decltype(auto) print_freestanding_decay_no_pack(outputstmtype optstm, Args... args)
{
	if constexpr (::fast_io::operations::decay::defines::has_status_print_define<outputstmtype>)
	{
		// Status-print streams handle the complete run through their stream-specific customization.
		return status_print_define<line>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		// Empty runs either emit only the requested newline or produce no output.
		if constexpr (line)
		{
			// The line variant of an empty run emits exactly one newline character.
			using char_type = typename outputstmtype::output_char_type;
			return ::fast_io::operations::decay::char_put_decay(optstm, char_literal_v<u8'\n', char_type>);
		}
		else
		{
			// The non-line variant of an empty run emits nothing.
			return;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outputstmtype>)
	{
		// Mutex-wrapped streams lock once and delegate output to the unlocked stream reference.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		return ::fast_io::operations::decay::print_freestanding_decay_no_pack<line>(
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm), args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
	{
		// Buffered streams use the put-area-aware control dispatcher.
		return ::fast_io::details::decay::print_controls_buffer_impl<line>(optstm, args...);
	}
	else
	{
		// Streams without a put area use the generic control dispatcher.
		return ::fast_io::details::decay::print_controls_impl<line>(optstm, args...);
	}
}

/// @brief    Forward declaration for semantic-aware freestanding emission.
/// @tparam   line              true when a trailing newline is appended
/// @tparam   already_forwarded true when inputs have already passed semantic forwarding
/// @tparam   char_type         the output character type
/// @tparam   outputstmtype     the decayed output stream reference type
/// @tparam   Args              the argument types in the print run
/// @param    optstm            the output stream reference
/// @param    args              the arguments to emit
template <bool line, bool already_forwarded, ::std::integral char_type, typename outputstmtype, typename... Args>
inline constexpr void print_semantic_emit(outputstmtype optstm, Args &&...args);

/// @brief    Writes a repeated fill character to a stream in fixed-size chunks.
/// @tparam   char_type     the output character type
/// @tparam   outputstmtype the decayed output stream reference type
/// @param    optstm        the output stream reference
/// @param    n             the number of fill characters to emit
/// @param    ch            the fill character
template <::std::integral char_type, typename outputstmtype>
inline constexpr void print_semantic_write_fill(outputstmtype optstm, ::std::size_t n, char_type ch)
{
	if (n == 0)
	{
		// Empty padding emits no output.
		return;
	}
	char_type buffer[64];
	::fast_io::details::my_fill_n(buffer, static_cast<::std::size_t>(64u), ch);
	while (static_cast<::std::size_t>(64u) < n)
	{
		// Full fill chunks are written until only a tail chunk remains.
		::fast_io::operations::decay::write_all_decay(optstm, buffer, buffer + 64);
		n -= static_cast<::std::size_t>(64u);
	}
	::fast_io::operations::decay::write_all_decay(optstm, buffer, buffer + n);
}

/// @brief    Forward declaration for measuring a semantic argument after input forwarding.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to measure
/// @return   ::std::size_t the precise emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_precise_size_arg(T &&t);

/// @brief    Computes the precise emitted size for a semantic leaf.
/// @details  Leaves use their most direct sizing protocol; reserve-scatters leaves materialize descriptors only to
///           sum the generated scatter lengths.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic leaf type
/// @param    t         the leaf to measure
/// @return   ::std::size_t the precise emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_precise_size_leaf(T &&t)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
	{
		// Null leaves emit no characters.
		return 0;
	}
	else if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		// Static precise reserve leaves expose their character count at compile time.
		return print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, value_type>);
	}
	else if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
	{
		// Run-time precise reserve leaves report their exact character count before emission.
		return print_reserve_precise_size(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::scatter_printable<char_type, value_type>)
	{
		// Scatter leaves already expose the single contiguous output range length.
		return print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t)).len;
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, value_type>)
	{
		// Reserve-scatters leaves must build their scatters before the total length can be summed.
		constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
		static_assert(sz.scatters_size != 0);
		constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
		constexpr bool stack_buffer_ok{
			::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type> &&
			::fast_io::details::decay::print_stack_buffer_size_within_limit<sz.scatters_size,
																			::fast_io::basic_io_scatter_t<char_type>>};
		if constexpr (stack_buffer_ok)
		{
			// Small reserve-scatters sizing uses stack descriptor and reserve storage.
			::fast_io::basic_io_scatter_t<char_type> scatters[sz.scatters_size];
			char_type buffer[reserve_buffer_size];
			auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters,
													  buffer, ::std::forward<T>(t))};
			::std::size_t len{};
			for (auto iter{scatters}; iter != result.scatters_pos_ptr; ++iter)
			{
				// Each produced scatter contributes its character length to the precise total.
				len = ::fast_io::details::intrinsics::add_or_overflow_die(len, iter->len);
			}
			return len;
		}
		else
		{
			// Large reserve-scatters sizing allocates descriptor and reserve storage before summing lengths.
			::fast_io::details::local_operator_new_array_ptr<::fast_io::basic_io_scatter_t<char_type>> scatters(
				sz.scatters_size);
			::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
			auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>,
													  scatters.ptr, buffer.ptr, ::std::forward<T>(t))};
			::std::size_t len{};
			for (auto iter{scatters.ptr}; iter != result.scatters_pos_ptr; ++iter)
			{
				// Each produced scatter contributes its character length to the precise total.
				len = ::fast_io::details::intrinsics::add_or_overflow_die(len, iter->len);
			}
			return len;
		}
	}
}

/// @brief    Normalizes a width placement value into its integral dispatch code.
/// @tparam   placement_type the placement enumeration or integral-like type
/// @param    placement      the placement value
/// @return   ::std::size_t  the normalized placement code
template <typename placement_type>
inline constexpr ::std::size_t print_semantic_width_placement_code(placement_type placement) noexcept
{
	return static_cast<::std::size_t>(placement);
}

/// @brief    Returns the fill character for a semantic width node.
/// @tparam   char_type            the output character type
/// @tparam   width_traits         the compile-time width trait bundle
/// @tparam   width_reference_type the width node reference type
/// @param    wr                   the width node reference
/// @return   char_type            the selected fill character
template <::std::integral char_type, typename width_traits, typename width_reference_type>
inline constexpr char_type print_semantic_width_fill_char(width_reference_type &&wr) noexcept
{
	if constexpr (width_traits::has_fill_char)
	{
		// Width nodes with an explicit fill character cast it to the active output character type.
		return static_cast<char_type>(wr.ch);
	}
	else
	{
		// Width nodes without an explicit fill character use a space.
		return ::fast_io::char_literal_v<u8' ', char_type>;
	}
}

/// @brief    Returns the width placement selected by static traits or by the node at run time.
/// @tparam   width_traits         the compile-time width trait bundle
/// @tparam   width_reference_type the width node reference type
/// @param    wr                   the width node reference
/// @return   auto                 the selected placement value
template <typename width_traits, typename width_reference_type>
inline constexpr auto print_semantic_width_placement(width_reference_type &&wr) noexcept
{
	if constexpr (width_traits::runtime_placement)
	{
		// Runtime-placement width nodes carry the placement value in the node itself.
		return wr.placement;
	}
	else
	{
		// Static-placement width nodes use the placement encoded in their traits.
		return width_traits::static_placement;
	}
}

/// @brief    Forward declaration for measuring a semantic argument's internal padding shift.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to inspect
/// @return   ::std::size_t the internal shift position, or zero when unavailable
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_internal_shift_arg(T &&t);

/// @brief    Computes the internal padding insertion point for a semantic node or leaf.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic node or leaf type
/// @param    t         the semantic node or leaf to inspect
/// @return   ::std::size_t the internal shift position, or zero when unavailable
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_internal_shift(T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		// Conditions measure the internal shift of the branch that will actually be emitted.
		if (node_ref.pred)
		{
			// The true branch is selected for internal padding placement.
			return ::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.t1);
		}
		// The false branch is selected for internal padding placement.
		return ::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.t2);
	}
	else if constexpr (::fast_io::details::print_pack<node_type> ||
					   ::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		// Packs and nested width nodes do not expose one stable leaf-level internal shift.
		return 0;
	}
	else
	{
		// Leaf nodes may provide a print_define_internal_shift customization.
		using value_type = ::std::remove_cvref_t<decltype(node_ref)>;
		if constexpr (::fast_io::printable_internal_shift<char_type, value_type>)
		{
			// The leaf customization reports the number of characters before internal padding.
			return print_define_internal_shift(::fast_io::io_reserve_type<char_type, value_type>,
											   ::std::forward<decltype(node_ref)>(node_ref));
		}
		else
		{
			// Leaves without an internal-shift customization fall back to ordinary right placement.
			return 0;
		}
	}
}

/// @brief    Applies semantic input forwarding before measuring internal padding shift.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to inspect
/// @return   ::std::size_t the internal shift position, or zero when unavailable
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_internal_shift_arg(T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_internal_shift<char_type>(
		::std::forward<decltype(forwarded)>(forwarded));
}

/// @brief    Continuation that accumulates precise sizes for expanded semantic pack elements.
/// @tparam   char_type the output character type
template <::std::integral char_type>
struct print_semantic_precise_size_pack_continuation
{
	::std::size_t *totalptr;

	/// @brief    Adds every expanded pack element size to the running precise total.
	/// @tparam   PackArgs the expanded pack element types
	/// @param    pack_args the expanded pack elements
	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		((*totalptr = ::fast_io::details::intrinsics::add_or_overflow_die(
			  *totalptr,
			  ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(
				  ::std::forward<PackArgs>(pack_args)))),
		 ...);
	}
};

/// @brief    Computes the precise emitted size for a semantic node or leaf.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic node or leaf type
/// @param    t         the semantic node or leaf to measure
/// @return   ::std::size_t the precise emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_precise_size(T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		// Packs sum the precise size of every stored element.
		::std::size_t total{};
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_precise_size_pack_continuation<char_type>{
				__builtin_addressof(total)});
		return total;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		// Conditions measure only the branch that will be emitted.
		if (node_ref.pred)
		{
			// The predicate selected the true branch for size calculation.
			return ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.t1);
		}
		else
		{
			// The predicate selected the false branch for size calculation.
			return ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.t2);
		}
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		// Width nodes measure the child and then account for the requested field width.
		using width_traits = ::fast_io::details::decay::print_semantic_width_traits<node_type>;
		::std::size_t const child_len{
			::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.reference)};
		::std::size_t const width{node_ref.width};
		auto const placement{
			::fast_io::operations::decay::print_semantic_width_placement<width_traits>(node_ref)};
		::std::size_t const placement_code{
			::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
		if (width <= child_len || placement_code == 0u)
		{
			// Disabled placement or an already-wide child emits only the child size.
			return child_len;
		}
		// Active placement pads the child up to the requested field width.
		return width;
	}
	else
	{
		// Leaf nodes use their leaf-specific precise-size protocol.
		return ::fast_io::operations::decay::print_semantic_precise_size_leaf<char_type>(
			::std::forward<decltype(node_ref)>(node_ref));
	}
}

/// @brief    Applies semantic input forwarding before precise-size measurement.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to measure
/// @return   ::std::size_t the precise emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_precise_size_arg(T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_precise_size<char_type>(
		::std::forward<decltype(forwarded)>(forwarded));
}

/// @brief    Forward declaration for unchecked semantic emission into an already-sized buffer.
/// @tparam   char_type the destination buffer character type
/// @tparam   T         the semantic node or leaf type
/// @param    iter      the current output cursor
/// @param    t         the semantic node or leaf to emit
/// @return   char_type* one past the emitted output
template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked(char_type *iter, T &&t);

/// @brief    Forwards one semantic print argument and emits it into an already-sized buffer.
/// @details  This wrapper preserves the semantic forwarding rules used by the checked emitters before entering the
///           unchecked node dispatcher.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   T          the argument type accepted by the semantic print layer
/// @param    iter       a pointer to the next output position in a buffer large enough for the argument
/// @param    t          the argument to print
/// @return   char_type* a pointer one past the emitted argument
template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked_arg(char_type *iter, T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_emit_unchecked<char_type>(
		iter, ::std::forward<decltype(forwarded)>(forwarded));
}

/// @brief    Caps condition-pack common factoring to keep deep user packs from forcing excessive inlining.
/// @details  Small packs benefit from shared-prefix or shared-suffix emission, while very large packs are left to the
///           original conditional output path to avoid code-size pressure.
inline constexpr ::std::size_t print_semantic_common_factor_pack_max_size{8u};

/// @brief    Treats two null semantic print nodes as a trivially equal common factor.
/// @param    <unnamed>  the first null node
/// @param    <unnamed>  the second null node
/// @return   bool       true because both nodes emit no output
inline constexpr bool print_semantic_common_factor_equal(::fast_io::io_null_t, ::fast_io::io_null_t) noexcept
{
	return true;
}

/// @brief    Compares two scatter-like semantic leaves for byte-for-byte identical output.
/// @details  A scatter leaf is a valid common factor only when both alternatives refer to the same base and, when the
///           extent is stored at run time, the same length.
/// @tparam   T     the first scatter-like node type
/// @tparam   U     the second scatter-like node type, constrained to the same decayed type as T
/// @param    first the first candidate common factor
/// @param    second the second candidate common factor
/// @return   bool  true when both leaves are safe to emit once before or after the conditional body
template <typename T, typename U>
	requires(!::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> &&
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<U>> &&
			 requires(T const &first, U const &second) {
				 first.base;
				 second.base;
			 })
inline constexpr bool print_semantic_common_factor_equal(T const &first, U const &second) noexcept
{
	if constexpr (requires {
					  first.len;
					  second.len;
				  })
	{
		// A dynamic scatter factor must match both the address and the emitted extent.
		return first.base == second.base && first.len == second.len;
	}
	else
	{
		// A static-extent scatter factor carries its extent in the type, so matching the address is sufficient.
		return first.base == second.base;
	}
}

/// @brief    Compares two integral reference semantic leaves for identical output.
/// @details  Value-like integral leaves can be factored when both condition alternatives carry the same reference
///           value and therefore emit the same characters.
/// @tparam   T     the first integral-reference node type
/// @tparam   U     the second integral-reference node type, constrained to the same decayed type as T
/// @param    first the first candidate common factor
/// @param    second the second candidate common factor
/// @return   bool  true when both leaves can be emitted on the shared path
template <typename T, typename U>
	requires(!::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> &&
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<U>> &&
			 requires(T const &first, U const &second) {
				 first.reference;
				 second.reference;
				 requires ::std::integral<::std::remove_cvref_t<decltype(first.reference)>>;
			 })
inline constexpr bool print_semantic_common_factor_equal(T const &first, U const &second) noexcept
{
	return first.reference == second.reference;
}

/// @brief    Detects whether two semantic leaves support conservative common-factor comparison.
/// @details  Only leaf kinds with an equality predicate that proves identical output participate in condition-pack
///           factoring; every other kind falls back to the original conditional emission.
/// @tparam   T the first leaf expression type
/// @tparam   U the second leaf expression type
template <typename T, typename U>
concept print_semantic_common_factor_comparable = requires(T &&t, U &&u) {
	{
		::fast_io::operations::decay::print_semantic_common_factor_equal(
			::std::forward<T>(t), ::std::forward<U>(u))
	} -> ::std::same_as<bool>;
};

/// @brief    Emits one element of a semantic pack into an already-sized buffer.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   index      the pack element index to emit
/// @tparam   Pack       the semantic pack type
/// @param    iter       a pointer to the next output position
/// @param    pack       the pack whose indexed element is printed
/// @return   char_type* a pointer one past the emitted element
template <::std::integral char_type, ::std::size_t index, typename Pack>
inline constexpr char_type *print_semantic_emit_unchecked_pack_element(char_type *iter, Pack &&pack)
{
	return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(
		iter, ::fast_io::containers::get<index>(::std::forward<Pack>(pack).storage));
}

/// @brief    Tests whether the same indexed element of two packs is a safe shared output factor.
/// @tparam   index the pack element index to compare
/// @tparam   Pack1 the first semantic pack type
/// @tparam   Pack2 the second semantic pack type
/// @param    pack1 the pack from the true branch
/// @param    pack2 the pack from the false branch
/// @return   bool  true when the indexed elements provably emit identical output
template <::std::size_t index, typename Pack1, typename Pack2>
inline constexpr bool print_semantic_pack_element_common_factor_equal(Pack1 &&pack1, Pack2 &&pack2)
{
	decltype(auto) first{::fast_io::containers::get<index>(::std::forward<Pack1>(pack1).storage)};
	decltype(auto) second{::fast_io::containers::get<index>(::std::forward<Pack2>(pack2).storage)};
	if constexpr (::fast_io::operations::decay::print_semantic_common_factor_comparable<decltype(first),
																						decltype(second)>)
	{
		// Comparable leaves may be shared when their equality predicate proves identical emitted bytes.
		return ::fast_io::operations::decay::print_semantic_common_factor_equal(
			::std::forward<decltype(first)>(first), ::std::forward<decltype(second)>(second));
	}
	else
	{
		// Non-comparable leaves stay inside their original conditional branch to preserve output semantics.
		return false;
	}
}

/// @brief    Emits a contiguous half-open range of semantic pack elements.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   first      the first element index to emit
/// @tparam   last       one past the final element index to emit
/// @tparam   Pack       the semantic pack type
/// @param    iter       a pointer to the next output position
/// @param    pack       the pack providing the emitted elements
/// @return   char_type* a pointer one past the emitted range
template <::std::integral char_type, ::std::size_t first, ::std::size_t last, typename Pack>
inline constexpr char_type *print_semantic_emit_unchecked_pack_range(char_type *iter, Pack &&pack)
{
	if constexpr (first < last)
	{
		// A non-empty range emits the front element and recursively advances the output cursor.
		iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_element<char_type, first>(
			iter, ::std::forward<Pack>(pack));
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_range<char_type, first + 1u, last>(
			iter, ::std::forward<Pack>(pack));
	}
	else
	{
		// An empty range emits nothing and returns the current output cursor unchanged.
		return iter;
	}
}

/// @brief    Emits a condition-pack range after at least one shared edge factor has been found.
/// @details  The routine recursively peels identical prefix and suffix elements so that only the irreducible middle
///           range remains guarded by the predicate.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   first      the first candidate element index
/// @tparam   last       one past the final candidate element index
/// @tparam   Pack1      the semantic pack type for the true branch
/// @tparam   Pack2      the semantic pack type for the false branch
/// @param    iter       a pointer to the next output position
/// @param    pred       the condition selecting the unfactored middle range
/// @param    pack1      the pack emitted when pred is true
/// @param    pack2      the pack emitted when pred is false
/// @return   char_type* a pointer one past the emitted condition-pack range
template <::std::integral char_type, ::std::size_t first, ::std::size_t last, typename Pack1, typename Pack2>
inline constexpr char_type *print_semantic_emit_unchecked_condition_pack_factored(char_type *iter, bool pred,
																				  Pack1 &&pack1, Pack2 &&pack2)
{
	if constexpr (first < last)
	{
		// A non-empty candidate range first tries to peel a shared prefix element.
		if (::fast_io::operations::decay::print_semantic_pack_element_common_factor_equal<first>(
				::std::forward<Pack1>(pack1), ::std::forward<Pack2>(pack2)))
		{
			// A shared prefix element is emitted once before the remaining conditional output.
			iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_element<char_type, first>(
				iter, ::std::forward<Pack1>(pack1));
			return ::fast_io::operations::decay::print_semantic_emit_unchecked_condition_pack_factored<
				char_type, first + 1u, last>(iter, pred, ::std::forward<Pack1>(pack1), ::std::forward<Pack2>(pack2));
		}
		if constexpr (first + 1u < last)
		{
			// A range with at least two elements can also peel a shared suffix element.
			constexpr ::std::size_t suffix_index{last - 1u};
			if (::fast_io::operations::decay::print_semantic_pack_element_common_factor_equal<suffix_index>(
					::std::forward<Pack1>(pack1), ::std::forward<Pack2>(pack2)))
			{
				// A shared suffix element is delayed until the conditional middle range has been emitted.
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_condition_pack_factored<
					char_type, first, suffix_index>(iter, pred, ::std::forward<Pack1>(pack1),
													::std::forward<Pack2>(pack2));
				return ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_element<char_type,
																								suffix_index>(
					iter, ::std::forward<Pack1>(pack1));
			}
		}
		if (pred)
		{
			// The predicate selected the true branch, so emit the remaining true-pack range verbatim.
			return ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_range<char_type, first, last>(
				iter, ::std::forward<Pack1>(pack1));
		}
		// The predicate selected the false branch, so emit the remaining false-pack range verbatim.
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_range<char_type, first, last>(
			iter, ::std::forward<Pack2>(pack2));
	}
	else
	{
		// An exhausted factored range has no more output to append.
		return iter;
	}
}

/// @brief    Carries the result of a condition-pack common-factor attempt.
/// @tparam   char_type the character type of the destination buffer
template <::std::integral char_type>
struct print_semantic_condition_pack_factor_result
{
	char_type *iter;
	bool done;
};

/// @brief    Attempts to start conservative common factoring for a condition whose alternatives are equal-sized packs.
/// @details  The attempt succeeds only when a shared prefix or suffix is found at the edge of the candidate range;
///           otherwise the caller must use the original conditional output path.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   first      the first candidate element index
/// @tparam   last       one past the final candidate element index
/// @tparam   Pack1      the semantic pack type for the true branch
/// @tparam   Pack2      the semantic pack type for the false branch
/// @param    iter       a pointer to the next output position
/// @param    pred       the condition selecting between the unfactored pack ranges
/// @param    pack1      the pack emitted when pred is true
/// @param    pack2      the pack emitted when pred is false
/// @return   print_semantic_condition_pack_factor_result<char_type> the updated cursor and success flag
template <::std::integral char_type, ::std::size_t first, ::std::size_t last, typename Pack1, typename Pack2>
inline constexpr print_semantic_condition_pack_factor_result<char_type>
print_semantic_emit_unchecked_condition_pack_try_factor(char_type *iter, bool pred, Pack1 &&pack1, Pack2 &&pack2)
{
	if constexpr (first < last)
	{
		// A non-empty candidate range can start factoring from either edge.
		if (::fast_io::operations::decay::print_semantic_pack_element_common_factor_equal<first>(
				::std::forward<Pack1>(pack1), ::std::forward<Pack2>(pack2)))
		{
			// A shared prefix starts the factored output path and proves the transformation is worthwhile.
			iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_element<char_type, first>(
				iter, ::std::forward<Pack1>(pack1));
			return {::fast_io::operations::decay::print_semantic_emit_unchecked_condition_pack_factored<
						char_type, first + 1u, last>(iter, pred, ::std::forward<Pack1>(pack1),
													 ::std::forward<Pack2>(pack2)),
					true};
		}
		if constexpr (first + 1u < last)
		{
			// A range with at least two elements can try a suffix factor after prefix factoring fails.
			constexpr ::std::size_t suffix_index{last - 1u};
			if (::fast_io::operations::decay::print_semantic_pack_element_common_factor_equal<suffix_index>(
					::std::forward<Pack1>(pack1), ::std::forward<Pack2>(pack2)))
			{
				// A shared suffix starts the factored path by emitting the conditional middle first.
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_condition_pack_factored<
					char_type, first, suffix_index>(iter, pred, ::std::forward<Pack1>(pack1),
													::std::forward<Pack2>(pack2));
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_pack_element<char_type,
																								suffix_index>(
					iter, ::std::forward<Pack1>(pack1));
				return {iter, true};
			}
		}
	}
	// No safe edge factor was found, so the caller must preserve the original conditional branch.
	return {iter, false};
}

/// @brief    Applies unchecked semantic emission to every element supplied by a pack expansion.
/// @tparam   char_type the character type of the destination buffer
template <::std::integral char_type>
struct print_semantic_emit_unchecked_pack_continuation
{
	char_type **iterptr;

	/// @brief    Emits every expanded pack element and updates the shared output cursor.
	/// @tparam   PackArgs the expanded pack element types
	/// @param    pack_args the expanded pack elements
	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		((*iterptr = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(
			  *iterptr, ::std::forward<PackArgs>(pack_args))),
		 ...);
	}
};

/// @brief    Emits a semantic leaf into an already-sized buffer.
/// @details  The dispatcher selects the most direct leaf protocol available: null output, reserve output, scatter
///           output, or reserve-scatters output.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   T          the semantic leaf type
/// @param    iter       a pointer to the next output position
/// @param    t          the leaf to emit
/// @return   char_type* a pointer one past the emitted leaf
template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked_leaf(char_type *iter, T &&t)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
	{
		// Null leaves intentionally emit no output and leave the cursor unchanged.
		return iter;
	}
	else if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		// Static precise reserve leaves know their full extent and can write directly to the current cursor.
		return print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, iter, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
	{
		// Precise reserve leaves compute their exact size before choosing the define protocol.
		::std::size_t const precise_size{
			print_reserve_precise_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
		if constexpr (requires {
						  {
							  print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
														   precise_size, ::std::forward<T>(t))
						  } -> ::std::same_as<char_type *>;
					  })
		{
			// Pointer-returning precise reserve leaves advance the cursor through their define operation.
			return print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
												precise_size, ::std::forward<T>(t));
		}
		else
		{
			// Void-returning precise reserve leaves rely on the measured size to advance the cursor.
			print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
										 precise_size, ::std::forward<T>(t));
			return iter + precise_size;
		}
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, value_type>)
	{
		// Dynamic reserve leaves have already been admitted by a bounded materialization path.
		return print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, iter, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		// Static reserve leaves have already been admitted by a bounded materialization path.
		return print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, iter, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::scatter_printable<char_type, value_type>)
	{
		// Scatter leaves copy their single contiguous range into the unchecked output buffer.
		auto scatter{print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t))};
		return ::fast_io::details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, value_type>)
	{
		// Reserve-scatters leaves materialize temporary descriptors before copying each segment.
		constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
		static_assert(sz.scatters_size != 0);
		constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
		constexpr bool stack_buffer_ok{
			::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type> &&
			::fast_io::details::decay::print_stack_buffer_size_within_limit<sz.scatters_size,
																			::fast_io::basic_io_scatter_t<char_type>>};
		if constexpr (stack_buffer_ok)
		{
			// Small reserve-scatters leaves materialize temporary scatter and reserve storage on the stack.
			::fast_io::basic_io_scatter_t<char_type> scatters[sz.scatters_size];
			char_type buffer[reserve_buffer_size];
			auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters,
													  buffer, ::std::forward<T>(t))};
			for (auto scatter_iter{scatters}; scatter_iter != result.scatters_pos_ptr; ++scatter_iter)
			{
				// Each produced scatter segment is copied into the unchecked contiguous output buffer.
				iter = ::fast_io::details::non_overlapped_copy_n(scatter_iter->base, scatter_iter->len, iter);
			}
			return iter;
		}
		else
		{
			// Large reserve-scatters leaves allocate temporary scatter and reserve storage before copying output.
			::fast_io::details::local_operator_new_array_ptr<::fast_io::basic_io_scatter_t<char_type>> scatters(
				sz.scatters_size);
			::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
			auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>,
													  scatters.ptr, buffer.ptr, ::std::forward<T>(t))};
			for (auto scatter_iter{scatters.ptr}; scatter_iter != result.scatters_pos_ptr; ++scatter_iter)
			{
				// Each produced scatter segment is copied into the unchecked contiguous output buffer.
				iter = ::fast_io::details::non_overlapped_copy_n(scatter_iter->base, scatter_iter->len, iter);
			}
			return iter;
		}
	}
}

/// @brief    Emits any semantic print node into an already-sized buffer.
/// @details  This unchecked dispatcher is used after precise sizing or coalescing has guaranteed enough contiguous
///           output space. It preserves semantic nodes such as packs, conditions, and width manipulators while
///           delegating leaves to the leaf protocol dispatcher.
/// @tparam   char_type  the character type of the destination buffer
/// @tparam   T          the semantic node or leaf type
/// @param    iter       a pointer to the next output position
/// @param    t          the node to emit
/// @return   char_type* a pointer one past the emitted node
template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked(char_type *iter, T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		// Packs emit each element in storage order into the same unchecked output run.
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_emit_unchecked_pack_continuation<char_type>{
				__builtin_addressof(iter)});
		return iter;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		// Condition nodes choose one alternative, with optional common factoring for small pack alternatives.
		using first_type = ::std::remove_cvref_t<decltype(node_ref.t1)>;
		using second_type = ::std::remove_cvref_t<decltype(node_ref.t2)>;
		if constexpr (::fast_io::details::print_pack<first_type> && ::fast_io::details::print_pack<second_type>)
		{
			// Condition alternatives that are both packs may expose equal edge elements for shared emission.
			if constexpr (first_type::size == second_type::size &&
						  first_type::size <=
							  ::fast_io::operations::decay::print_semantic_common_factor_pack_max_size)
			{
				// Equal-sized small packs try conservative common factoring before falling back to the raw predicate.
				auto const factor_result{
					::fast_io::operations::decay::print_semantic_emit_unchecked_condition_pack_try_factor<
						char_type, 0u, first_type::size>(iter, node_ref.pred, node_ref.t1, node_ref.t2)};
				if (factor_result.done)
				{
					// A successful factor attempt has already emitted the complete condition-pack output.
					return factor_result.iter;
				}
				if (node_ref.pred)
				{
					// The predicate selected the true pack after no safe shared factor was found.
					return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter,
																									  node_ref.t1);
				}
				// The predicate selected the false pack after no safe shared factor was found.
				return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t2);
			}
			else
			{
				// Large or unequal pack alternatives skip factoring and use the predicate directly.
				if (node_ref.pred)
				{
					// Large or unequal pack alternatives are emitted through the true branch without factoring.
					return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter,
																									  node_ref.t1);
				}
				// Large or unequal pack alternatives are emitted through the false branch without factoring.
				return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t2);
			}
		}
		else
		{
			// Non-pack alternatives are emitted by the predicate without common-factor analysis.
			if (node_ref.pred)
			{
				// Non-pack condition alternatives emit the true branch selected by the predicate.
				return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t1);
			}
			// Non-pack condition alternatives emit the false branch selected by the predicate.
			return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t2);
		}
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		// Width nodes materialize their child before padding so dynamic-reserve children use their actual length.
		using width_traits = ::fast_io::details::decay::print_semantic_width_traits<node_type>;
		::std::size_t const width{node_ref.width};
		char_type const fillch{
			::fast_io::operations::decay::print_semantic_width_fill_char<char_type, width_traits>(node_ref)};
		auto const placement{
			::fast_io::operations::decay::print_semantic_width_placement<width_traits>(node_ref)};
		::std::size_t const placement_code{
			::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
		char_type *const first{iter};
		char_type *const last{
			::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference)};
		::std::size_t const len{static_cast<::std::size_t>(last - first)};
		if (width <= len || placement_code == 0u)
		{
			// Width that is already satisfied, or disabled placement, leaves the child bytes unchanged.
			return last;
		}
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			// Left placement appends all padding after the already-emitted child.
			return ::fast_io::details::my_fill_n(last, padding, fillch);
		}
		if (placement_code == 2u)
		{
			// Center placement shifts the child right, then fills both sides in place.
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			char_type *const child_pos{first + left_padding};
			::fast_io::details::my_copy(first, last, child_pos);
			::fast_io::details::my_fill_n(first, left_padding, fillch);
			::fast_io::details::my_fill_n(child_pos + len, right_padding, fillch);
			return first + width;
		}
		if (placement_code == 4u)
		{
			// Internal placement inserts padding after the sign or prefix reported by the child.
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.reference)};
			if (internal_len != 0 && internal_len <= len)
			{
				// A valid internal shift allows padding to be inserted inside the already-emitted child bytes.
				char_type *const shift_pos{first + internal_len};
				::fast_io::details::my_copy(shift_pos, last, shift_pos + padding);
				::fast_io::details::my_fill_n(shift_pos, padding, fillch);
				return first + width;
			}
		}
		// Right placement, or internal placement without a shift point, moves the child behind leading padding.
		::fast_io::details::my_copy(first, last, first + padding);
		::fast_io::details::my_fill_n(first, padding, fillch);
		return first + width;
	}
	else
	{
		// Leaf nodes use the fastest printable protocol supported by their decayed type.
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_leaf<char_type>(
			iter, ::std::forward<decltype(node_ref)>(node_ref));
	}
}

/// @brief    Computes the total static precise size for a semantic print run.
/// @details  The value is available only when every argument has a compile-time precise semantic size.
/// @tparam   line      true when a trailing newline is part of the run
/// @tparam   char_type the character type of the output run
/// @tparam   Args      the argument types in the run
/// @return   ::std::size_t the static total size, or SIZE_MAX when any argument is not statically sized
template <bool line, ::std::integral char_type, typename... Args>
inline consteval ::std::size_t print_semantic_static_precise_total_size() noexcept
{
	if constexpr ((::fast_io::details::decay::print_semantic_static_precise_size<char_type, Args>::available && ...))
	{
		// A fully static run can sum all compile-time sizes and feed fixed-size stack coalescing.
		::std::size_t total{};
		((total = ::fast_io::details::intrinsics::add_or_overflow_die(
			  total, ::fast_io::details::decay::print_semantic_static_precise_size<char_type, Args>::size)),
		 ...);
		if constexpr (line)
		{
			// The line variant reserves one additional character for the trailing newline.
			total = ::fast_io::details::intrinsics::add_or_overflow_die(total, static_cast<::std::size_t>(1u));
		}
		return total;
	}
	else
	{
		// A non-static argument prevents fixed-size coalescing at compile time.
		return SIZE_MAX;
	}
}

/// @brief    Computes the run-time precise size for a semantic print run.
/// @details  This is used by checked coalescing paths before allocating or reserving contiguous output space.
/// @tparam   line      true when a trailing newline is part of the run
/// @tparam   char_type the character type of the output run
/// @tparam   Args      the argument types in the run
/// @param    args      the arguments whose semantic sizes are measured
/// @return   ::std::size_t the total number of characters needed by the run
template <bool line, ::std::integral char_type, typename... Args>
inline constexpr ::std::size_t print_semantic_precise_total_size(Args &&...args)
{
	::std::size_t total{};
	((total = ::fast_io::details::intrinsics::add_or_overflow_die(
		  total, ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(
					 args))),
	 ...);
	if constexpr (line)
	{
		// The line variant includes the final newline in the measured run.
		total = ::fast_io::details::intrinsics::add_or_overflow_die(total, static_cast<::std::size_t>(1u));
	}
	return total;
}

/// @brief    Emits a precise semantic print run into an already-sized contiguous buffer.
/// @tparam   line      true when a trailing newline is appended
/// @tparam   char_type the character type of the destination buffer
/// @tparam   Args      the argument types in the run
/// @param    iter      a pointer to the next output position
/// @param    args      the arguments to emit in order
/// @return   char_type* a pointer one past the emitted run
template <bool line, ::std::integral char_type, typename... Args>
inline constexpr char_type *print_semantic_emit_unchecked_run(char_type *iter, Args &&...args)
{
	((iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(
		  iter, ::std::forward<Args>(args))),
	 ...);
	if constexpr (line)
	{
		// The line variant appends a newline after the last emitted argument.
		*iter = ::fast_io::char_literal_v<u8'\n', char_type>;
		++iter;
	}
	return iter;
}

/// @brief    Forward declaration for computing a bounded semantic argument size after input forwarding.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to measure
/// @return   ::std::size_t an upper bound for the emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_bounded_size_arg(T &&t);

/// @brief    Computes an emitted-size upper bound for a semantic leaf.
/// @details  Precise leaves use their exact size; dynamic-reserve leaves use their object-specific reserve bound.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic leaf type
/// @param    t         the leaf to measure
/// @return   ::std::size_t an upper bound for the emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_bounded_size_leaf(T &&t)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, value_type>)
	{
		// Existing precise leaves already provide an exact bound.
		return ::fast_io::operations::decay::print_semantic_precise_size_leaf<char_type>(::std::forward<T>(t));
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, value_type>)
	{
		// Dynamic reserve leaves report the maximum buffer size needed for this object.
		return print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		// Static reserve leaves expose a compile-time object-independent upper bound.
		return print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>);
	}
}

/// @brief    Continuation that accumulates bounded sizes for expanded semantic pack elements.
/// @tparam   char_type the output character type
template <::std::integral char_type>
struct print_semantic_bounded_size_pack_continuation
{
	::std::size_t *totalptr;

	/// @brief    Adds every expanded pack element upper bound to the running total.
	/// @tparam   PackArgs the expanded pack element types
	/// @param    pack_args the expanded pack elements
	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		((*totalptr = ::fast_io::details::intrinsics::add_or_overflow_die(
			  *totalptr,
			  ::fast_io::operations::decay::print_semantic_bounded_size_arg<char_type>(
				  ::std::forward<PackArgs>(pack_args)))),
		 ...);
	}
};

/// @brief    Computes an emitted-size upper bound for a semantic node or leaf.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic node or leaf type
/// @param    t         the semantic node or leaf to measure
/// @return   ::std::size_t an upper bound for the emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_bounded_size(T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		// Packs sum the upper bound of every stored element.
		::std::size_t total{};
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_bounded_size_pack_continuation<char_type>{
				__builtin_addressof(total)});
		return total;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		// Conditions need only the branch selected for this object.
		if (node_ref.pred)
		{
			return ::fast_io::operations::decay::print_semantic_bounded_size_arg<char_type>(node_ref.t1);
		}
		return ::fast_io::operations::decay::print_semantic_bounded_size_arg<char_type>(node_ref.t2);
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		// Width nodes use the larger of the child upper bound and the active field width.
		using width_traits = ::fast_io::details::decay::print_semantic_width_traits<node_type>;
		::std::size_t const child_bound{
			::fast_io::operations::decay::print_semantic_bounded_size_arg<char_type>(node_ref.reference)};
		::std::size_t const width{node_ref.width};
		auto const placement{
			::fast_io::operations::decay::print_semantic_width_placement<width_traits>(node_ref)};
		::std::size_t const placement_code{
			::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
		if (width <= child_bound || placement_code == 0u)
		{
			return child_bound;
		}
		return width;
	}
	else
	{
		// Leaf nodes use either an exact size or a dynamic reserve bound.
		return ::fast_io::operations::decay::print_semantic_bounded_size_leaf<char_type>(
			::std::forward<decltype(node_ref)>(node_ref));
	}
}

/// @brief    Applies semantic input forwarding before bounded-size measurement.
/// @tparam   char_type the output character type
/// @tparam   T         the semantic input argument type
/// @param    t         the argument to measure
/// @return   ::std::size_t an upper bound for the emitted character count
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_bounded_size_arg(T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_bounded_size<char_type>(
		::std::forward<decltype(forwarded)>(forwarded));
}

/// @brief    Computes the run-time bounded size for a semantic print run.
/// @tparam   line      true when a trailing newline is part of the run
/// @tparam   char_type the character type of the output run
/// @tparam   Args      the argument types in the run
/// @param    args      the arguments whose semantic bounds are measured
/// @return   ::std::size_t the total upper bound needed by the run
template <bool line, ::std::integral char_type, typename... Args>
inline constexpr ::std::size_t print_semantic_bounded_total_size(Args &&...args)
{
	::std::size_t total{};
	((total = ::fast_io::details::intrinsics::add_or_overflow_die(
		  total, ::fast_io::operations::decay::print_semantic_bounded_size_arg<char_type>(args))),
	 ...);
	if constexpr (line)
	{
		// The line variant includes the final newline in the measured bound.
		total = ::fast_io::details::intrinsics::add_or_overflow_die(total, static_cast<::std::size_t>(1u));
	}
	return total;
}

/// @brief    Attempts to coalesce a bounded semantic print run into one contiguous output operation.
/// @details  This path admits dynamic-reserve leaves by allocating for their upper bound and writing the actual length.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   Args          the argument types in the run
/// @param    optstm        the output stream reference
/// @param    args          the arguments to emit
/// @return   bool          true when the whole run was emitted by this coalescing path
template <bool line, ::std::integral char_type, typename outputstmtype, typename... Args>
inline constexpr bool print_semantic_try_bounded_coalesce(outputstmtype optstm, Args &&...args)
{
	if constexpr ((::fast_io::details::decay::print_semantic_bounded_size_ok<
					   char_type,
					   ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
				   ...))
	{
		// A bounded run can be materialized once when its upper bound fits the available coalescing space.
		::std::size_t const required{
			::fast_io::operations::decay::print_semantic_bounded_total_size<line, char_type>(args...)};
		if (required == 0)
		{
			return true;
		}
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
		{
			// Buffered streams can use the existing put area when it can hold the upper bound.
			char_type *const curr{obuffer_curr(optstm)};
			char_type *const end{obuffer_end(optstm)};
			if (static_cast<::std::size_t>(end - curr) >= required)
			{
				char_type *const iter{
					::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
						curr, ::std::forward<Args>(args)...)};
				obuffer_set_curr(optstm, iter);
				return true;
			}
		}
		constexpr ::std::size_t threshold_chars{
			::fast_io::details::decay::print_full_output_coalesce_threshold<char_type, outputstmtype>()};
		if constexpr (threshold_chars != 0)
		{
			// Unbuffered streams use a stack buffer when the upper bound is below their full-output threshold.
			if (required <= threshold_chars)
			{
				char_type buffer[threshold_chars];
				char_type *const iter{
					::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
						buffer, ::std::forward<Args>(args)...)};
				::fast_io::operations::decay::write_all_decay(optstm, buffer, iter);
				return true;
			}
			constexpr ::std::size_t materialize_limit{
				::fast_io::details::decay::print_stack_buffer_max_size<char_type>()};
			if constexpr (materialize_limit != 0)
			{
				if (required <= materialize_limit)
				{
					::fast_io::details::buffer_alloc_arr_ptr<char_type, false> buffer(required);
					char_type *const first{buffer.get()};
					char_type *const last{
						::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
							first, ::std::forward<Args>(args)...)};
					::fast_io::operations::decay::write_all_decay(optstm, first, last);
					return true;
				}
			}
		}
	}
	// At least one requirement for bounded coalescing failed, so the caller must use the normal emit path.
	return false;
}

/// @brief    Attempts to coalesce a precise semantic print run into one contiguous output operation.
/// @details  The function first uses existing output-buffer space when available, then falls back to a bounded stack
///           buffer according to the full-output coalescing threshold.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   Args          the argument types in the run
/// @param    optstm        the output stream reference
/// @param    args          the arguments to emit
/// @return   bool          true when the whole run was emitted by this coalescing path
template <bool line, ::std::integral char_type, typename outputstmtype, typename... Args>
inline constexpr bool print_semantic_try_precise_coalesce(outputstmtype optstm, Args &&...args)
{
	if constexpr ((::fast_io::details::decay::print_semantic_precise_size_ok<
					   char_type,
					   ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
				   ...))
	{
		// A fully precise run can be measured before choosing a coalesced output strategy.
		::std::size_t const required{
			::fast_io::operations::decay::print_semantic_precise_total_size<line, char_type>(
				args...)};
		if (required == 0)
		{
			// An empty precise run completes without touching the output stream.
			return true;
		}
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
		{
			// Buffered streams can coalesce directly into their current put area when enough space remains.
			char_type *const curr{obuffer_curr(optstm)};
			char_type *const end{obuffer_end(optstm)};
			if (static_cast<::std::size_t>(end - curr) >= required)
			{
				// The existing output buffer has enough room, so emit in place and advance the stream cursor once.
				char_type *const iter{
					::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
						curr, ::std::forward<Args>(args)...)};
				obuffer_set_curr(optstm, iter);
				return true;
			}
		}
		constexpr ::std::size_t threshold_chars{
			::fast_io::details::decay::print_full_output_coalesce_threshold<char_type, outputstmtype>()};
		if constexpr (threshold_chars != 0)
		{
			// A non-zero full-output threshold permits bounded stack-buffer coalescing.
			constexpr ::std::size_t static_total{
				::fast_io::operations::decay::print_semantic_static_precise_total_size<line, char_type,
																					   ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>...>()};
			if constexpr (static_total != SIZE_MAX && static_total <= threshold_chars)
			{
				// A compile-time bounded run uses an exactly-sized stack buffer and one write operation.
				constexpr ::std::size_t buffer_size{static_total == 0 ? 1u : static_total};
				char_type buffer[buffer_size];
				char_type *const iter{
					::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
						buffer, ::std::forward<Args>(args)...)};
				::fast_io::operations::decay::write_all_decay(optstm, buffer, iter);
				return true;
			}
			else
			{
				// Non-static precise runs can still coalesce when the measured size fits the stack threshold.
				if (required <= threshold_chars)
				{
					// A run-time bounded run uses the threshold-sized stack buffer and one write operation.
					char_type buffer[threshold_chars];
					char_type *const iter{
						::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, char_type>(
							buffer, ::std::forward<Args>(args)...)};
					::fast_io::operations::decay::write_all_decay(optstm, buffer, iter);
					return true;
				}
			}
		}
	}
	// At least one requirement for precise coalescing failed, so the caller must use the normal emit path.
	return false;
}

/// @brief    Emits a width-formatted semantic child using the most direct available output path.
/// @details  The function first tries the stream put area, then a small stack buffer, and finally a streaming fallback
///           that writes padding and child output in sequence.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the child reference type
/// @param    optstm        the output stream reference
/// @param    reference     the child node or leaf to format
/// @param    width         the requested minimum field width
/// @param    fillch        the padding character
/// @param    placement_code the normalized width placement code
/// @param    len           the precise child length
template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
inline constexpr void print_semantic_emit_width_direct(outputstmtype optstm, T &&reference, ::std::size_t width,
													   char_type fillch, ::std::size_t placement_code,
													   ::std::size_t len)
{
	if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
	{
		// Buffered streams first try to build the entire width-formatted field in the existing put area.
		::std::size_t required{(width <= len || placement_code == 0u) ? len : width};
		if constexpr (line)
		{
			// The line variant needs one additional output slot for the trailing newline.
			required = ::fast_io::details::intrinsics::add_or_overflow_die(required, static_cast<::std::size_t>(1u));
		}
		char_type *const curr{obuffer_curr(optstm)};
		char_type *const end{obuffer_end(optstm)};
		if (static_cast<::std::size_t>(end - curr) >= required)
		{
			// The stream put area can hold the complete field, so emit directly and commit once.
			char_type *iter{curr};
			if (width <= len || placement_code == 0u)
			{
				// Direct-buffer width is already satisfied, so only the child output is emitted.
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
			}
			else
			{
				// Direct-buffer padding is required before the formatted field can be committed.
				::std::size_t const padding{width - len};
				if (placement_code == 1u)
				{
					// Direct-buffer left placement appends all padding after the child output.
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
				}
				else if (placement_code == 2u)
				{
					// Direct-buffer center placement emits left padding, child output, then right padding.
					::std::size_t const left_padding{padding >> 1u};
					::std::size_t const right_padding{padding - left_padding};
					iter = ::fast_io::details::my_fill_n(iter, left_padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, right_padding, fillch);
				}
				else if (placement_code == 4u)
				{
					// Direct-buffer internal placement inserts padding after the child's sign or prefix.
					::std::size_t const internal_len{
						::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(reference)};
					if (internal_len == 0)
					{
						// Without an internal shift point, internal placement falls back to right placement.
						iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
						iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter,
																										  reference);
					}
					else
					{
						// A valid internal shift point lets the already-emitted suffix move right to make room.
						char_type *const first{iter};
						char_type *const last{
							::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference)};
						if (len < internal_len)
						{
							// An invalid internal span preserves the child output and avoids padding insertion.
							iter = last;
						}
						else
						{
							// The suffix is shifted right and the internal padding is filled in place.
							char_type *const shift_pos{first + internal_len};
							::fast_io::details::my_copy(shift_pos, last, shift_pos + padding);
							::fast_io::details::my_fill_n(shift_pos, padding, fillch);
							iter = first + width;
						}
					}
				}
				else
				{
					// Direct-buffer right placement emits all padding before the child output.
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
				}
			}
			if constexpr (line)
			{
				// The line variant terminates the completed field with a newline.
				*iter++ = ::fast_io::char_literal_v<u8'\n', char_type>;
			}
			obuffer_set_curr(optstm, iter);
			return;
		}
	}
	::std::size_t required{(width <= len || placement_code == 0u) ? len : width};
	if constexpr (line)
	{
		// The fallback sizing path also accounts for the optional trailing newline.
		required = ::fast_io::details::intrinsics::add_or_overflow_die(required, static_cast<::std::size_t>(1u));
	}
	constexpr ::std::size_t small_width_buffer_size{
		(::fast_io::details::decay::print_stack_buffer_max_size<char_type>() < static_cast<::std::size_t>(256u))
			? ::fast_io::details::decay::print_stack_buffer_max_size<char_type>()
			: static_cast<::std::size_t>(256u)};
	if constexpr (small_width_buffer_size != 0u)
	{
		// A non-zero small-width buffer enables a stack-buffer fallback for compact formatted fields.
		if (required <= small_width_buffer_size)
		{
			// Small width-formatted fields use a bounded stack buffer and one stream write.
			char_type buffer[small_width_buffer_size];
			char_type *iter{buffer};
			if (width <= len || placement_code == 0u)
			{
				// Stack-buffer width is already satisfied, so only the child output is emitted.
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
			}
			else
			{
				// Stack-buffer padding is required before the formatted field can be written once.
				::std::size_t const padding{width - len};
				if (placement_code == 1u)
				{
					// Stack-buffer left placement appends all padding after the child output.
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
				}
				else if (placement_code == 2u)
				{
					// Stack-buffer center placement emits left padding, child output, then right padding.
					::std::size_t const left_padding{padding >> 1u};
					::std::size_t const right_padding{padding - left_padding};
					iter = ::fast_io::details::my_fill_n(iter, left_padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, right_padding, fillch);
				}
				else if (placement_code == 4u)
				{
					// Stack-buffer internal placement inserts padding after the child's sign or prefix.
					::std::size_t const internal_len{
						::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(reference)};
					if (internal_len == 0)
					{
						// Without an internal shift point, internal placement falls back to right placement.
						iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
						iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter,
																										  reference);
					}
					else
					{
						// A valid internal shift point lets the already-emitted suffix move right to make room.
						char_type *const first{iter};
						char_type *const last{
							::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference)};
						if (len < internal_len)
						{
							// An invalid internal span preserves the child output and avoids padding insertion.
							iter = last;
						}
						else
						{
							// The suffix is shifted right and the internal padding is filled in place.
							char_type *const shift_pos{first + internal_len};
							::fast_io::details::my_copy(shift_pos, last, shift_pos + padding);
							::fast_io::details::my_fill_n(shift_pos, padding, fillch);
							iter = first + width;
						}
					}
				}
				else
				{
					// Stack-buffer right placement emits all padding before the child output.
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
				}
			}
			if constexpr (line)
			{
				// The line variant terminates the stack-buffered field with a newline.
				*iter++ = ::fast_io::char_literal_v<u8'\n', char_type>;
			}
			::fast_io::operations::decay::write_all_decay(optstm, buffer, iter);
			return;
		}
	}
	if (width <= len || placement_code == 0u)
	{
		// Streaming fallback width is already satisfied, so emit the child directly to the stream.
		::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
	}
	else
	{
		// Streaming fallback padding is written directly around the child output.
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			// Streaming fallback left placement writes the child first and then the trailing padding.
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
		}
		else if (placement_code == 2u)
		{
			// Streaming fallback center placement writes leading padding, child output, and trailing padding.
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			::fast_io::operations::decay::print_semantic_write_fill(optstm, left_padding, fillch);
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, right_padding, fillch);
		}
		else if (placement_code == 4u)
		{
			// Streaming fallback internal placement may need a temporary child buffer for byte insertion.
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(reference)};
			if (internal_len == 0)
			{
				// Without an internal shift point, internal placement falls back to right placement.
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			}
			else
			{
				// The child is buffered so padding can be inserted between its internal prefix and suffix.
				::fast_io::basic_dynamic_output_buffer<char_type> buffer;
				auto buffer_ref{::fast_io::operations::output_stream_ref(buffer)};
				::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(buffer_ref, reference);
				char_type const *const first{buffer.begin_ptr};
				char_type const *const last{buffer.curr_ptr};
				if (len < internal_len)
				{
					// An invalid internal span preserves the buffered child output without inserting padding.
					::fast_io::operations::decay::write_all_decay(optstm, first, last);
				}
				else
				{
					// The stream receives the internal prefix, padding, and remaining child suffix in order.
					::fast_io::operations::decay::write_all_decay(optstm, first, first + internal_len);
					::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
					::fast_io::operations::decay::write_all_decay(optstm, first + internal_len, last);
				}
			}
		}
		else
		{
			// Streaming fallback right placement writes leading padding before the child output.
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
		}
	}
	if constexpr (line)
	{
		// The line variant appends the newline after the streaming fallback field.
		::fast_io::operations::decay::char_put_decay(optstm, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

/// @brief    Emits every element of a semantic pack through the checked stream emitter.
/// @tparam   line          true when the last emitted element should carry the caller's line policy
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
template <bool line, ::std::integral char_type, typename outputstmtype>
struct print_semantic_emit_node_pack_continuation
{
	outputstmtype optstm;

	/// @brief    Emits all expanded pack elements through the checked semantic emitter.
	/// @tparam   PackArgs the expanded pack element types
	/// @param    pack_args the expanded pack elements
	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(
			optstm, ::std::forward<PackArgs>(pack_args)...);
	}
};

/// @brief    Emits a semantic width node to an output stream.
/// @details  Precise-size children use the direct width emitter; non-precise children are first materialized into a
///           dynamic buffer so width placement can be applied accurately.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the width semantic node type
/// @param    optstm        the output stream reference
/// @param    t             the width node to emit
template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
inline constexpr void print_semantic_emit_width(outputstmtype optstm, T &&t)
{
	auto &&width_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using width_type = ::std::remove_cvref_t<decltype(width_ref)>;
	using width_traits = ::fast_io::details::decay::print_semantic_width_traits<width_type>;
	using width_child_type =
		::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, decltype(width_ref.reference)>;
	::std::size_t const width{width_ref.width};
	char_type const fillch{
		::fast_io::operations::decay::print_semantic_width_fill_char<char_type, width_traits>(width_ref)};
	auto const placement{::fast_io::operations::decay::print_semantic_width_placement<width_traits>(width_ref)};
	::std::size_t const placement_code{
		::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
	if constexpr (::fast_io::details::decay::print_semantic_static_precise_size<char_type,
																				width_child_type>::available)
	{
		// Static precise children can pass a compile-time length directly into the width emitter.
		constexpr ::std::size_t len{
			::fast_io::details::decay::print_semantic_static_precise_size<char_type, width_child_type>::size};
		::fast_io::operations::decay::print_semantic_emit_width_direct<line, char_type>(
			optstm, width_ref.reference, width, fillch, placement_code, len);
		return;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_precise_size_ok<char_type, width_child_type>::value)
	{
		// Run-time precise children are measured once and then emitted through the direct width path.
		::std::size_t const len{
			::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(width_ref.reference)};
		::fast_io::operations::decay::print_semantic_emit_width_direct<line, char_type>(
			optstm, width_ref.reference, width, fillch, placement_code, len);
		return;
	}
	// Non-precise children are buffered so the final child length is known before padding is written.
	::fast_io::basic_dynamic_output_buffer<char_type> buffer;
	auto buffer_ref{::fast_io::operations::output_stream_ref(buffer)};
	::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(buffer_ref, width_ref.reference);
	char_type const *const first{buffer.begin_ptr};
	char_type const *const last{buffer.curr_ptr};
	::std::size_t const len{static_cast<::std::size_t>(last - first)};
	if (width <= len || placement_code == 0u)
	{
		// Buffered fallback width is already satisfied, so the child bytes are forwarded unchanged.
		::fast_io::operations::decay::write_all_decay(optstm, first, last);
	}
	else
	{
		// Buffered fallback padding is written around the materialized child bytes.
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			// Buffered fallback left placement writes the child bytes followed by trailing padding.
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
		}
		else if (placement_code == 2u)
		{
			// Buffered fallback center placement writes leading padding, child bytes, and trailing padding.
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			::fast_io::operations::decay::print_semantic_write_fill(optstm, left_padding, fillch);
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, right_padding, fillch);
		}
		else if (placement_code == 4u)
		{
			// Buffered fallback internal placement uses the child's reported prefix length for insertion.
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(width_ref.reference)};
			if (internal_len == 0)
			{
				// Without an internal shift point, internal placement falls back to right placement.
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::write_all_decay(optstm, first, last);
			}
			else if (len < internal_len)
			{
				// An invalid internal span preserves the buffered child output without inserting padding.
				::fast_io::operations::decay::write_all_decay(optstm, first, last);
			}
			else
			{
				// The stream receives the buffered internal prefix, padding, and suffix in order.
				::fast_io::operations::decay::write_all_decay(optstm, first, first + internal_len);
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::write_all_decay(optstm, first + internal_len, last);
			}
		}
		else
		{
			// Buffered fallback right placement writes leading padding before the child bytes.
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
		}
	}
	if constexpr (line)
	{
		// The line variant appends a newline after the buffered fallback field.
		::fast_io::operations::decay::char_put_decay(optstm, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

/// @brief    Emits one semantic node to an output stream.
/// @details  The checked node dispatcher handles packs, conditions, and width nodes before returning control to the
///           flattened semantic emitter.
/// @tparam   line          true when this node is responsible for the caller's trailing newline
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   T             the semantic node type
/// @param    optstm        the output stream reference
/// @param    t             the semantic node to emit
template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
inline constexpr void print_semantic_emit_node(outputstmtype optstm, T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		// Pack nodes are expanded and each element is emitted through the checked stream path.
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_emit_node_pack_continuation<line, char_type, outputstmtype>{
				optstm});
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		// Checked condition nodes dispatch exactly the branch selected by the predicate.
		if (node_ref.pred)
		{
			// Condition nodes emit the true alternative selected by the predicate.
			::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(optstm, node_ref.t1);
		}
		else
		{
			// Condition nodes emit the false alternative selected by the predicate.
			::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(optstm, node_ref.t2);
		}
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		// Width nodes delegate to the width-specific emitter so padding placement remains centralized.
		::fast_io::operations::decay::print_semantic_emit_width<line, char_type>(optstm,
																				 ::std::forward<T>(t));
	}
}

/// @brief    Invokes the accumulated flattened semantic continuation with the final line policy.
/// @tparam   line          true when the flattened run appends a trailing newline
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   continuation  the accumulated prefix continuation type
template <bool line, ::std::integral char_type, typename outputstmtype, typename continuation>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit_flat_impl(outputstmtype, continuation &&cont)
{
	cont.template operator()<line>();
}

/// @brief    Accumulates non-node arguments while flattening a semantic print run.
/// @tparam   continuation the downstream flattened continuation type
/// @tparam   T            the argument type captured as part of the prefix
template <typename continuation, typename T>
struct print_semantic_emit_flat_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	/// @brief    Prepends the captured plain argument to the flattened prefix.
	/// @tparam   prefix_line true when the prefix should append a newline
	/// @tparam   Prefix      the already-accumulated prefix argument types
	/// @param    prefix      the already-accumulated prefix arguments
	template <bool prefix_line, typename... Prefix>
	inline constexpr void operator()(Prefix &&...prefix) const
	{
		contptr->template operator()<prefix_line>(::std::forward<T>(*valueptr),
												  ::std::forward<Prefix>(prefix)...);
	}
};

/// @brief    Flattens a semantic print run until the next semantic node is reached.
/// @details  Plain arguments are accumulated into the prefix continuation; once a node is found, the prefix is emitted,
///           the node is handled, and flattening resumes for the remaining arguments.
/// @tparam   line          true when the final flattened run appends a trailing newline
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   continuation  the accumulated prefix continuation type
/// @tparam   T             the current argument type
/// @tparam   Args          the remaining argument types
template <bool line, ::std::integral char_type, typename outputstmtype, typename continuation, typename T,
		  typename... Args>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit_flat_impl(outputstmtype optstm, continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::decay::print_semantic_node<T>)
	{
		// A semantic node terminates the current flat prefix before node-specific output is emitted.
		cont.template operator()<false>();
		::fast_io::operations::decay::print_semantic_emit_node<false, char_type>(optstm, ::std::forward<T>(t));
		::fast_io::operations::decay::print_semantic_emit<line, true, char_type>(optstm,
																				 ::std::forward<Args>(args)...);
	}
	else
	{
		// A plain argument is captured into the prefix so later coalescing can treat it as one flat run.
		::fast_io::operations::decay::print_semantic_emit_flat_impl<line, char_type>(
			optstm,
			::fast_io::operations::decay::print_semantic_emit_flat_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

/// @brief    Emits the flattened prefix through the ordinary freestanding printing machinery.
/// @tparam   outputstmtype the decayed output stream reference type
template <typename outputstmtype>
struct print_semantic_emit_freestanding_continuation
{
	outputstmtype optstm;

	/// @brief    Emits the accumulated flattened prefix through the best freestanding path.
	/// @tparam   prefix_line true when this prefix should append a newline
	/// @tparam   Prefix      the flattened prefix argument types
	/// @param    prefix      the flattened prefix arguments
	template <bool prefix_line, typename... Prefix>
	inline constexpr void operator()(Prefix &&...prefix) const
	{
		using char_type = typename outputstmtype::output_char_type;
		if constexpr (::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry_available<
						  char_type, ::std::remove_cvref_t<Prefix>...>())
		{
			// The fast dynamic reserve-scatters entry handles the prefix when every argument supports it.
			::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry<prefix_line>(
				optstm, ::std::forward<Prefix>(prefix)...);
		}
		else
		{
			// The generic no-pack path emits prefixes that do not match the reserve-scatters fast entry.
			::fast_io::operations::decay::print_freestanding_decay_no_pack<prefix_line>(
				optstm, ::std::forward<Prefix>(prefix)...);
		}
	}
};

/// @brief    Entry continuation for flattened semantic emission.
/// @details  It tries precise and bounded coalescing for the whole filtered run before semantic flattening.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   char_type     the character type of the output stream
/// @tparam   outputstmtype the decayed output stream reference type
template <bool line, ::std::integral char_type, typename outputstmtype>
struct print_semantic_emit_flat_continuation
{
	outputstmtype optstm;

	/// @brief    Emits a filtered semantic run, preferring coalescing before flattening.
	/// @tparam   FilteredArgs the filtered argument types
	/// @param    filtered_args the filtered arguments
	template <typename... FilteredArgs>
	inline constexpr void operator()(FilteredArgs &&...filtered_args) const
	{
		if (!::fast_io::operations::decay::print_semantic_try_precise_coalesce<line, char_type>(
				optstm, ::std::forward<FilteredArgs>(filtered_args)...) &&
			!::fast_io::operations::decay::print_semantic_try_bounded_coalesce<line, char_type>(
				optstm, ::std::forward<FilteredArgs>(filtered_args)...))
		{
			// Failed coalescing keeps semantics by flattening nodes around ordinary freestanding output.
			::fast_io::operations::decay::print_semantic_emit_flat_impl<line, char_type>(
				optstm,
				::fast_io::operations::decay::print_semantic_emit_freestanding_continuation<outputstmtype>{optstm},
				::std::forward<FilteredArgs>(filtered_args)...);
		}
	}
};

/// @brief    Filters null semantic outputs before the flattened emitter is invoked.
/// @tparam   emit_flat_type the flattened emitter continuation type
template <typename emit_flat_type>
struct print_semantic_filter_flat_continuation
{
	::std::remove_reference_t<emit_flat_type> *emitflatptr;

	/// @brief    Removes null outputs from a flattened semantic run before emission.
	/// @tparam   FlatArgs the flattened argument types
	/// @param    flat_args the flattened arguments
	template <typename... FlatArgs>
	inline constexpr void operator()(FlatArgs &&...flat_args) const
	{
		::fast_io::details::decay::print_semantic_filter_nulls<false, false>(
			*emitflatptr, ::std::forward<FlatArgs>(flat_args)...);
	}
};

/// @brief    Emits a semantic-aware freestanding print run.
/// @details  The entry expands semantic packs and nulls when needed, then routes the resulting flat run through
///           coalescing and semantic node emission.
/// @tparam   line              true when a trailing newline is appended
/// @tparam   already_forwarded true when inputs have already passed semantic input forwarding
/// @tparam   char_type         the character type of the output stream
/// @tparam   outputstmtype     the decayed output stream reference type
/// @tparam   Args              the argument types in the print run
/// @param    optstm            the output stream reference
/// @param    args              the arguments to emit
template <bool line, bool already_forwarded, ::std::integral char_type, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit(outputstmtype optstm, Args &&...args)
{
	print_semantic_emit_flat_continuation<line, char_type, outputstmtype> emit_flat{optstm};
	if constexpr (!((::fast_io::details::decay::print_semantic_pack_argument_v<Args> ||
					 ::std::same_as<::std::remove_cvref_t<Args>, ::fast_io::io_null_t>) ||
					...))
	{
		// A run without semantic pack arguments or nulls can enter the flattened emitter directly.
		emit_flat(::std::forward<Args>(args)...);
	}
	else
	{
		// Pack arguments and nulls are expanded or removed before the flattened emitter sees the run.
		::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::fast_io::operations::decay::print_semantic_filter_flat_continuation<decltype(emit_flat)>{
				__builtin_addressof(emit_flat)},
			::std::forward<Args>(args)...);
	}
}

/// @brief    Freestanding print dispatcher for already-decayed stream references.
/// @details  This function selects status printing, empty-line handling, mutex unlocking, semantic emission, or the
///           regular non-semantic freestanding paths.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   Args          the argument types in the print run
/// @param    optstm        the output stream reference
/// @param    args          the arguments to emit
/// @return   decltype(auto) the return value of the selected output customization, when any
template <bool line, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_freestanding_decay(outputstmtype optstm, Args... args)
{
	if constexpr (::fast_io::operations::decay::defines::has_status_print_define<outputstmtype>)
	{
		// Status-print streams delegate the whole freestanding run to their status customization.
		return status_print_define<line>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		// Empty decayed runs either emit only the requested newline or produce no output.
		if constexpr (line)
		{
			// An empty line-print run emits exactly one newline character.
			using char_type = typename outputstmtype::output_char_type;
			return ::fast_io::operations::decay::char_put_decay(optstm, char_literal_v<u8'\n', char_type>);
		}
		else
		{
			// An empty non-line print run emits nothing.
			return;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outputstmtype>)
	{
		// Mutex-wrapped streams lock once and then print through the unlocked stream reference.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		return ::fast_io::operations::decay::print_freestanding_decay<line>(
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm), args...);
	}
	else if constexpr ((::fast_io::details::decay::print_semantic_node<Args> || ...))
	{
		// Semantic nodes require the semantic-aware emitter before falling back to ordinary leaf output.
		using char_type = typename outputstmtype::output_char_type;
		return ::fast_io::operations::decay::print_semantic_emit<line, true, char_type>(optstm, args...);
	}
	else
	{
		// Non-semantic non-empty runs choose between the fast reserve/scatter entry and the generic path.
		using char_type = typename outputstmtype::output_char_type;
		if constexpr (::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry_available<
						  char_type, ::std::remove_cvref_t<Args>...>())
		{
			// Non-semantic arguments use the dynamic reserve-scatters fast entry when it is available.
			return ::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry<line>(optstm,
																									   args...);
		}
		else
		{
			// Non-semantic arguments without the fast entry use the general no-pack freestanding path.
			return ::fast_io::operations::decay::print_freestanding_decay_no_pack<line>(optstm, args...);
		}
	}
}

/// @brief    Cold wrapper around the freestanding print dispatcher.
/// @details  This keeps unlikely front-end call sites out of the hot dispatcher body on compilers that support cold
///           placement or branch hints.
/// @tparam   line          true when a trailing newline is appended
/// @tparam   outputstmtype the decayed output stream reference type
/// @tparam   Args          the argument types in the print run
/// @param    optstm        the output stream reference
/// @param    args          the arguments to emit
/// @return   decltype(auto) the return value of the selected output customization, when any
template <bool line, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr decltype(auto) print_freestanding_decay_cold(outputstmtype optstm, Args... args)
{
#if !__has_cpp_attribute(__gnu__::__cold__) && __has_cpp_attribute(unlikely)
	if (true) [[unlikely]]
#endif
		return ::fast_io::operations::decay::print_freestanding_decay<line>(optstm, args...);
}

} // namespace decay

namespace decay::defines
{

template <typename char_type, typename... Args>
concept print_freestanding_params_okay =
	::std::integral<char_type> &&
	(::fast_io::details::decay::print_freestanding_decay_param_okay_single<char_type, Args>::value && ...);

template <typename output, typename... Args>
concept print_freestanding_okay =
	::fast_io::operations::decay::defines::print_freestanding_params_okay<typename output::output_char_type, Args...>;

} // namespace decay::defines

namespace defines
{

template <typename char_type, typename... Args>
concept print_freestanding_params_okay = ::fast_io::operations::decay::defines::print_freestanding_params_okay<char_type,
																											   decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<Args>())))...>;

template <typename output, typename... Args>
concept print_freestanding_okay = ::fast_io::operations::decay::defines::print_freestanding_okay<
	decltype(::fast_io::operations::output_stream_ref(::std::declval<output>())),
	decltype(::fast_io::io_print_forward<typename decltype(::fast_io::operations::output_stream_ref(
				 ::std::declval<output>()))::output_char_type>(::fast_io::io_print_alias(::std::declval<Args>())))...>;

} // namespace defines

/// @brief    Public freestanding print entry point.
/// @details  The wrapper obtains the stream reference, forwards semantic input arguments when present, and then
///           delegates to the decayed freestanding dispatcher.
/// @tparam   line   true when a trailing newline is appended
/// @tparam   output the output stream object type
/// @tparam   Args   the argument types in the print run
/// @param    outstm the output stream object
/// @param    args   the arguments to emit
template <bool line, typename output, typename... Args>
inline constexpr void print_freestanding(output &&outstm, Args &&...args)
{
	auto outref{::fast_io::operations::output_stream_ref(outstm)};
	using char_type = typename decltype(outref)::output_char_type;
	if constexpr ((false || ... ||
				   ::fast_io::details::decay::print_semantic_input_argument_v<char_type, Args &&>))
	{
		// Semantic input arguments are forwarded into their semantic representation before decayed emission.
		::fast_io::operations::decay::print_freestanding_decay<line>(
			outref,
			::fast_io::details::decay::print_semantic_input_forward<char_type>(
				::std::forward<Args>(args))...);
	}
	else
	{
		// Ordinary arguments use the regular alias and print-forward path before decayed emission.
		::fast_io::operations::decay::print_freestanding_decay<line>(
			outref, io_print_forward<char_type>(io_print_alias(args))...);
	}
}

} // namespace operations

} // namespace fast_io

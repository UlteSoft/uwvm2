#pragma once

/*
 * Freestanding scan execution pipeline (IO operation core).
 *
 * The public IO facade first obtains an input stream reference and applies
 * `io_scan_alias`/`io_scan_forward` to each target. This file then owns the
 * complete scan record: stable proxy storage, input mutex acquisition,
 * whole-record `status_scan_define` dispatch, buffered chunk/refill control,
 * and selection among precise-reserve, contiguous, terminal-padding, and
 * context-scanner protocols. Returned cursors and parse codes are validated
 * and normalized before the caller observes success, EOF, or an exception.
 *
 * This file neither parses a format language nor defines primitive input
 * devices. `operations::decay` means that stream/source normalization has
 * already occurred; lower read/ibuffer CPOs provide bytes and scanner CPOs
 * define how one target consumes them. The `report` behavior of the user-facing
 * `io::scan` facade remains above this level.
 */

namespace fast_io
{

namespace operations::decay
{

/// @brief Measured code-generation budgets for optional value transport of normalized scan proxies.
/// @details GCC 15 on x86-64 SysV kept 32 one-word entry parameters inlined, while a 48-proxy owner crossed its
///          inlining limit and emitted stack argument setup in every hot iteration. The whole-pack owner therefore
///          retains its measured 32-object ceiling; other ABIs use a smaller conservative, currently unmeasured
///          fallback bound. These counts are inlining/text-size policy, not a claim that a machine has that many
///          argument registers. The same object and rounded-byte limits bound each independently outlined cold
///          fallback chunk below. Keeping one set of constants makes the size proof compositional: neither entry
///          ownership nor chunk transport exceeds the selected call-shape budget.
#if defined(__linux__) && defined(__x86_64__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr ::std::size_t scan_owned_proxy_max_count{32u};
#else
inline constexpr ::std::size_t scan_owned_proxy_max_count{16u};
#endif
// The per-object ceiling follows the argument side of the target model rather than assuming SysV's two-eightbyte
// envelope on Microsoft x64. AAPCS64 and SysV AMD64 retain the two-word opportunity; MS x64 admits only its direct
// scalar-sized aggregate envelope. Semantic transport still requires an explicit marker and trivial special members.
inline constexpr ::std::size_t scan_owned_proxy_max_object_size{
	::fast_io::details::abi_small_trivial_argument_max_size};
inline constexpr ::std::size_t scan_owned_proxy_max_total_size{
	scan_owned_proxy_max_count * sizeof(::std::size_t)};

/// @brief Conservative object count for one cold by-value fallback call.
/// @details Every admitted object is at most `scan_owned_proxy_max_object_size`; multiplying that worst case by this
///          count can therefore never exceed the rounded-byte budget. A one-word homogeneous pack could use a wider
///          chunk, but cold-path call count is less important than keeping this proof independent of heterogeneous pack
///          order and avoiding another recursive size classifier. Large packs use multiple chunks, so every call shape
///          remains bounded while compile-time and emitted work stay linear in the number of scanner arguments.
inline constexpr ::std::size_t scan_proxy_value_fallback_chunk_max_count{
	(scan_owned_proxy_max_total_size / scan_owned_proxy_max_object_size) <
			scan_owned_proxy_max_count
		? (scan_owned_proxy_max_total_size / scan_owned_proxy_max_object_size)
		: scan_owned_proxy_max_count};
static_assert(scan_proxy_value_fallback_chunk_max_count != 0u);
static_assert(scan_proxy_value_fallback_chunk_max_count * scan_owned_proxy_max_object_size <=
			  scan_owned_proxy_max_total_size);

} // namespace operations::decay

namespace details
{

/// @brief Recognizes an ordinary whole-terminal scan path.
/// @details A terminal-padding CPO is an optional accelerator layered on this ordinary protocol, never a replacement
///          for it. Consequently the common input/scanner admission rule has exactly the pre-padding shape.
template <typename input, typename char_type, typename T>
concept input_contiguous_scannable =
	::std::integral<char_type> &&
	(::fast_io::contiguous_scannable<char_type, T> ||
	 ::fast_io::contiguous_padding_scannable_protocol<char_type, T>);

/// @brief Invokes the strongest contiguous scanning protocol jointly supported by the input and scanner.
/// @details Let S=[first,last), and, when available, P=`contiguous_range_padding_size(in)`.  The padded branch receives
///          `(S,P)` but its protocol requires a result cursor in `[first,last]` and padding noninterference.  The
///          ordinary branch receives S alone.  Consequently both branches implement the same abstract scan; only the
///          padded branch may issue reads in `[last,last+P)`.  The caller still validates and commits against `last`,
///          so this helper cannot publish a cursor into padding.
template <typename input, ::std::integral char_type, typename T>
	requires input_contiguous_scannable<input, char_type, T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *> scan_contiguous_invoke(
	input &in, char_type const *first, char_type const *last, T &arg)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	if constexpr (
		::fast_io::contiguous_range_with_padding<input> &&
		::fast_io::terminal_contiguous_padding_scannable<char_type, T>)
	{
		/*
		The conjunction proves both halves of the composed contract: the input
		provides a readable physical suffix of P elements, and the scanner promises
		to mask all semantics to the true end.  Query P exactly once so a custom
		provider cannot make one scan observe inconsistent physical limits.
		*/
		using input_type = ::std::remove_cvref_t<input>;
		auto const padding{contiguous_range_padding_size(
			static_cast<input_type const &>(in))};
		if (padding != 0u) [[likely]]
		{
			if constexpr (::fast_io::contiguous_padding_scannable_define<char_type, T>)
			{
				return scan_contiguous_padding_define(
					io_reserve_type<char_type, scanner_type>, first, last,
					padding, arg);
			}
			else
			{
				return scan_contiguous_define(
					io_reserve_type<char_type, scanner_type>, first, last,
					padding, arg);
			}
		}
	}
	/*
	Padding is an opt-in terminal-tail acceleration.  An ordinary input type, a
	scanner without the accelerator, or an explicitly padded view carrying P==0
	all reach the original CPO with its original ABI.
	*/
	if constexpr (::fast_io::contiguous_scannable<char_type, T>)
	{
		return scan_contiguous_define(
			io_reserve_type<char_type, scanner_type>, first, last, arg);
	}
	else
	{
		// A padding-only scanner has no ordinary four-argument fallback.  A
		// zero-padding query still invokes its exact protocol, preserving the
		// provider's terminal semantics without forming a different CPO shape.
		if constexpr (::fast_io::contiguous_padding_scannable_define<char_type, T>)
		{
			return scan_contiguous_padding_define(
				io_reserve_type<char_type, scanner_type>, first, last,
				::std::size_t{}, arg);
		}
		else
		{
			return scan_contiguous_define(
				io_reserve_type<char_type, scanner_type>, first, last,
				::std::size_t{}, arg);
		}
	}
}

/// @brief Commits a scanner iterator only after proving membership in the current chunk.
template <typename input, typename current_pointer, ::std::integral char_type>
inline constexpr bool scan_commit_iterator_if_in_current_chunk(
	input &in, current_pointer current_pointer_value, char_type const *first,
	char_type const *last, char_type const *result)
{
	if (!::fast_io::details::scan_iterator_in_current_chunk(first, last, result)) [[unlikely]]
	{
		return false;
	}
	if (result == first)
	{
		// A terminal empty view may be represented by the equal pair `nullptr, nullptr`. Equality is well-defined for
		// that representation, but neither subtracting the two pointers nor adding zero to the backend's null cursor is
		// valid C++ pointer arithmetic. A zero-offset commit is exactly the old cursor, so publish that value directly.
		// Keeping the `ibuffer_set_curr` call also preserves the observable commit protocol used by custom ibuffers.
		ibuffer_set_curr(in, current_pointer_value);
		return true;
	}
	::std::size_t offset{};
	if (::std::is_constant_evaluated())
	{
		// Constant evaluation already proved ordinary array membership above, so language pointer subtraction is valid.
		offset = static_cast<::std::size_t>(result - first);
	}
	else
	{
		// A malformed customization can return a pointer with unrelated C++ provenance whose numeric address happens to
		// fall inside this chunk. The address/alignment predicate rejects ordinary escapes, but it cannot turn such a
		// pointer into a member of `first`'s array. Derive the offset from validated integer addresses instead of invoking
		// undefined pointer subtraction; applying that bounded offset to the real backend cursor retains its exact type
		// and provenance.
		offset = static_cast<::std::size_t>(
			(reinterpret_cast<::std::uintptr_t>(result) - reinterpret_cast<::std::uintptr_t>(first)) /
			sizeof(char_type));
	}
	ibuffer_set_curr(in, current_pointer_value + offset);
	return true;
}

/// @brief Publishes a scanner cursor after its leaf CPO has proved closed-range provenance.
/// @details The marker admitting this helper proves `result` belongs to the same array as `[first,last]`, so ordinary
///          pointer subtraction is defined.  Exact pointer backends take the still cheaper direct assignment.  Keeping
///          this operation separate prevents the generic address/alignment validator from entering trusted hot paths.
template <typename input, typename current_pointer, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_commit_bounded_iterator(
	input &in, current_pointer current_pointer_value, char_type const *first,
	char_type const *result)
{
	if constexpr (::std::same_as<current_pointer, char_type const *>)
	{
		ibuffer_set_curr(in, result);
	}
	else
	{
		ibuffer_set_curr(in, current_pointer_value + (result - first));
	}
}

/// @brief Executes and commits one terminal contiguous scan.
/// @details `scan_contiguous_invoke` proves protocol selection preserves `last` as semantic EOF.  The closed-interval
///          validator below independently checks the customization's returned address before converting it to the
///          backend cursor type.  Thus even a malformed padded scanner cannot commit into its readable suffix.  Status
///          normalization is identical for ordinary and padded scanners: ok succeeds, terminal exhaustion reports
///          false, and every other code throws.
template <typename input, typename T>
	requires ::fast_io::details::input_contiguous_scannable<
		input, typename input::input_char_type, T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_contiguous_status_impl(input &in, T &arg)
{
	using char_type = typename input::input_char_type;
	auto curr{ibuffer_curr(in)};
	auto end{ibuffer_end(in)};
	auto [it, ec] = ::fast_io::details::scan_contiguous_invoke(
		in, static_cast<char_type const *>(curr),
		static_cast<char_type const *>(end), arg);
	if constexpr (
		::fast_io::contiguous_scanner_result_in_range<char_type, T> &&
		!::fast_io::contiguous_range_with_padding<input>)
	{
		::fast_io::details::scan_commit_bounded_iterator(
			in, curr, static_cast<char_type const *>(curr), it);
	}
	else if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(
				 in, curr, static_cast<char_type const *>(curr),
				 static_cast<char_type const *>(end), it)) [[unlikely]]
	{
		/*
		The scanner violated its closed semantic-cursor contract.  Rejecting the
		result before cursor publication protects both the true file position and
		the padding boundary.
		*/
		throw_parse_code(parse_code::invalid);
	}
	if (ec == parse_code::ok)
	{
		/*
		A validated successful cursor has already been committed and lies at or
		before the true end, so the public reporting result is success.
		*/
		return true;
	}
	if (ec == parse_code::end_of_file || ec == parse_code::partial)
	{
		/*
		This function is called only after the input proves the current chunk is
		terminal.  No refill can complete `partial`, so both terminal-short statuses
		map to the existing false reporting result.
		*/
		return false;
	}
	/*
	All remaining codes preserve their ordinary exception behavior.  Padding
	changes legal load width only and cannot weaken error propagation.
	*/
	throw_parse_code(ec);
}

template <typename char_type, typename P, typename state_type>
concept scan_context_eof_rewindable = requires(state_type &state, P arg) {
	{
		scan_context_eof_rewind_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<P>>, state, arg)
	} -> ::std::same_as<::std::size_t>;
};

template <typename input, typename P, typename state_type, typename iter_type>
inline constexpr void scan_context_eof_rewind_if_available(input &in, state_type &state, P &arg,
														   iter_type chunk_begin, iter_type chunk_end)
{
	using char_type = typename input::input_char_type;
	if constexpr (::fast_io::details::scan_context_eof_rewindable<char_type, P, state_type>)
	{
		auto const rewind{static_cast<::std::size_t>(
			scan_context_eof_rewind_size(
				io_reserve_type<char_type, ::std::remove_cvref_t<P>>, state, arg))};
		if (rewind)
		{
			// A refill implementation may replace or invalidate its old logical end even when the refill reports EOF.
			// Reinstalling an iterator from that old span would then create curr > end. Rewind is valid only when the
			// input object still exposes the same terminal chunk after the failed underflow.
			if (ibuffer_end(in) != chunk_end)
			{
				return;
			}
			auto const chunk_size{static_cast<::std::size_t>(chunk_end - chunk_begin)};
			auto const rewinded{rewind < chunk_size ? chunk_end - rewind : chunk_begin};
			ibuffer_set_curr(in, rewinded);
		}
	}
}

/// @brief Copies exactly `count` buffered characters without converting clean EOF into an exception.
/// @details The public reporting scan API represents insufficient input as `false`. Generic `read_all` instead throws
///          on a short refill and, for a terminal view without read CPOs, can have no viable cold operation at all.
///          This helper consumes the already-required ibuffer protocol directly. Pointer subtraction occurs only after
///          a successful nonempty-buffer observation, so a freshly constructed `{nullptr,nullptr}` buffer is safe.
template <typename input>
inline constexpr bool scan_read_exact_from_ibuffer(
	input &in, typename input::input_char_type *destination, ::std::size_t count)
{
	while (count != 0u)
	{
		auto curr{ibuffer_curr(in)};
		auto end{ibuffer_end(in)};
		if (curr == end)
		{
			if (!ibuffer_underflow(in)) [[unlikely]]
			{
				return false;
			}
			curr = ibuffer_curr(in);
			end = ibuffer_end(in);
			if (curr == end) [[unlikely]]
			{
				// `true` is the underflow protocol's proof that at least one character became available.
				throw_parse_code(parse_code::invalid);
			}
		}
		::std::size_t const available{static_cast<::std::size_t>(end - curr)};
		::std::size_t const take{count < available ? count : available};
		destination = ::fast_io::details::non_overlapped_copy_n(curr, take, destination);
		ibuffer_set_curr(in, curr + take);
		count -= take;
	}
	return true;
}

/// @brief Applies one fixed-extent scanner and normalizes its two permitted result contracts to bool/exception.
template <::std::integral char_type, typename T>
inline constexpr bool scan_precise_reserve_apply(char_type const *buffer, T &arg)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	// Protocol admission observes the exact cv-qualified lvalue passed to the CPO. The reserve tag remains
	// unqualified by design; erasing cv from the argument during selection admitted mutable-only overloads for const
	// proxies and rejected overload sets intentionally constrained to const scanner views.
	if constexpr (::fast_io::precise_reserve_scannable_no_error<char_type, T>)
	{
		scan_precise_reserve_define(io_reserve_type<char_type, scanner_type>, buffer, arg);
		return true;
	}
	else
	{
		auto const code{scan_precise_reserve_define(io_reserve_type<char_type, scanner_type>, buffer, arg)};
		if (code == parse_code::ok)
		{
			return true;
		}
		if (code == parse_code::end_of_file)
		{
			return false;
		}
		throw_parse_code(code);
	}
}

/// @brief Returns the maximum hot-stack staging extent for a precise scanner, in character units.
/// @details Scan shares the build-wide stack budget but imposes an additional 4-KiB hot-frame ceiling. Large protocol
///          extents are type properties, not evidence that every caller can afford an equally large frame. Zero in the
///          configured budget disables stack staging and therefore selects the dynamic helper below. The policy is a
///          template argument, rather than an invisible default used inside a `char_type`-only specialization: because
///          `default_print_stack_policy` is `print_stack_policy<N>`, the chosen byte budget is present in the mangled
///          identity and two configured builds cannot COMDAT-fold capacity functions with different return values.
template <::std::integral char_type,
		  typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t scan_precise_inline_staging_capacity() noexcept
{
	constexpr ::std::size_t configured_bytes{
		::fast_io::details::print_stack_buffer_max_bytes<stack_policy>()};
	constexpr ::std::size_t preferred_bytes{4096u};
	constexpr ::std::size_t admitted_bytes{
		configured_bytes < preferred_bytes ? configured_bytes : preferred_bytes};
	return admitted_bytes / sizeof(char_type);
}

template <::std::size_t extent, typename input, typename T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr bool scan_precise_reserve_large_staging(input &in, T &arg)
{
	using char_type = typename input::input_char_type;
	static_assert(extent != 0u);
	// The allocation size is the scanner's exact type-level extent. Isolating it here prevents a large alternative
	// from inflating the hot scan dispatcher's frame while retaining ASan's exact-size staging boundary. This helper is
	// intentionally policy-free: its caller has already proved that dynamic storage is required, after which every
	// stack budget has identical allocation, read, and apply semantics and may safely share this noinline body.
	::fast_io::details::local_operator_new_array_ptr<char_type> buffer(extent);
	if (!::fast_io::details::scan_read_exact_from_ibuffer(in, buffer.ptr, extent))
	{
		return false;
	}
	return ::fast_io::details::scan_precise_reserve_apply<char_type>(buffer.ptr, arg);
}

template <::std::size_t extent,
		  typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename T>
inline constexpr bool scan_precise_reserve_staging(input &in, T &arg)
{
	// This specialization is the policy-dependent allocation decision. Encoding `stack_policy` here is required even
	// though `extent` is already a template value: the same scanner extent may be inline under one budget and dynamic
	// under another, so an extent-only inline function would have two non-equivalent definitions with one weak symbol.
	using char_type = typename input::input_char_type;
	if constexpr (extent == 0u)
	{
		// A valid object address avoids zero-length arrays and nullptr arithmetic. A zero-extent scanner is forbidden
		// from reading the character; it may still initialize or validate its target as a useful semantic operation.
		char_type dummy{};
		return ::fast_io::details::scan_precise_reserve_apply<char_type>(__builtin_addressof(dummy), arg);
	}
	else if constexpr (extent <=
					   ::fast_io::details::scan_precise_inline_staging_capacity<char_type, stack_policy>())
	{
		char_type buffer[extent];
		if (!::fast_io::details::scan_read_exact_from_ibuffer(in, buffer, extent))
		{
			return false;
		}
		return ::fast_io::details::scan_precise_reserve_apply<char_type>(buffer, arg);
	}
	else
	{
		return ::fast_io::details::scan_precise_reserve_large_staging<extent>(in, arg);
	}
}

#if 0
template<typename input,typename T,typename P>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr bool scan_single_status_impl(input in,T& state_machine,P arg)
{
	for(;state_machine.code==parse_code::partial;)
	{
		if(!ibuffer_underflow(in))
		{
			if(!state_machine.test_eof(arg))
				return false;
			if(state_machine.code==parse_code{})[[likely]]
				return true;
			break;
		}
		auto curr{ibuffer_curr(in)};
		auto end{ibuffer_end(in)};
		state_machine(curr,end,arg);
		ibuffer_set_curr(in, state_machine.iter);
		if(state_machine.code==parse_code::ok)[[likely]]
			return true;
	}
	throw_parse_code(state_machine.code);
}
#endif

/// @brief Runs the ordinary context-scanner state machine with caller-owned state storage.
/// @details This is the normal success path for a context-only scanner and must not be marked `cold`; doing so would
///          turn a valid concept choice into a layout and branch-prediction penalty. Storage policy is deliberately
///          outside the loop so a large state cannot inflate this hot frame.
template <typename input, typename P, typename state_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_context_status_with_state(input &in, P &arg, state_type &state)
{
	using char_type = typename input::input_char_type;
	using scanner_type = ::std::remove_cvref_t<P>;
	// `scan_context_define` may consume a chunk in several partial phases. Preserve the beginning of the logical chunk
	// across those calls: an EOF rewind is measured against the complete terminal chunk, not merely the suffix supplied
	// to the final phase. A successful refill establishes a new pair and is the only ordinary reset point.
	auto active_chunk_begin{ibuffer_curr(in)};
	auto active_chunk_end{ibuffer_end(in)};
	for (;;)
	{
		auto curr{ibuffer_curr(in)};
		auto end{ibuffer_end(in)};
		if (curr == end)
		{
			if (!ibuffer_underflow(in)) [[unlikely]]
			{
				auto const code{scan_context_eof_define(
					io_reserve_type<char_type, scanner_type>, state, arg)};
				if (code == parse_code::ok)
				{
					return true;
				}
				if (code == parse_code::end_of_file)
				{
					return false;
				}
				if (code == parse_code::partial) [[unlikely]]
				{
					// No future refill exists, so EOF cannot legally preserve an unfinished state.
					throw_parse_code(parse_code::invalid);
				}
				throw_parse_code(code);
			}
			curr = ibuffer_curr(in);
			end = ibuffer_end(in);
			if (curr == end) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			active_chunk_begin = curr;
			active_chunk_end = end;
		}
		auto [it, ec] = scan_context_define(
			io_reserve_type<char_type, scanner_type>, state, curr, end, arg);
		if constexpr (::fast_io::context_scanner_result_in_range<char_type, P>)
		{
			::fast_io::details::scan_commit_bounded_iterator(
				in, curr, static_cast<char_type const *>(curr), it);
		}
		else if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(
					 in, curr, static_cast<char_type const *>(curr),
					 static_cast<char_type const *>(end), it)) [[unlikely]]
		{
			// Validate before committing: no parse code makes an out-of-range iterator a valid cursor.
			throw_parse_code(parse_code::invalid);
		}
		if (ec == parse_code::ok)
		{
			return true;
		}
		if (ec == parse_code::end_of_file)
		{
			return false;
		}
		if (ec != parse_code::partial)
		{
			throw_parse_code(ec);
		}
		if (it != end)
		{
			// A partial scanner may deliberately leave input for another phase of the same state machine. Continue on
			// that suffix instead of discarding it through a refill. No progress with available input violates the
			// protocol and would otherwise spin forever.
			if (it == curr) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			continue;
		}
		if (!ibuffer_underflow(in)) [[unlikely]]
		{
			ec = scan_context_eof_define(io_reserve_type<char_type, scanner_type>, state, arg);
			if (ec == parse_code::ok)
			{
				return true;
			}
			if (ec == parse_code::end_of_file)
			{
				return false;
			}
			if (ec == parse_code::partial) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			::fast_io::details::scan_context_eof_rewind_if_available(
				in, state, arg, active_chunk_begin, active_chunk_end);
			throw_parse_code(ec);
		}
		// A successful underflow is the backend's progress proof. Validate it at this boundary rather than returning to
		// the loop head, where another underflow call could turn a broken `true + empty` backend into an unbounded loop.
		auto const refill_curr{ibuffer_curr(in)};
		auto const refill_end{ibuffer_end(in)};
		if (refill_curr == refill_end) [[unlikely]]
		{
			throw_parse_code(parse_code::invalid);
		}
		active_chunk_begin = refill_curr;
		active_chunk_end = refill_end;
	}
}

template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename P>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_context_status_impl(input &in, P &arg)
{
	using char_type = typename input::input_char_type;
	using scanner_type = ::std::remove_cvref_t<P>;
	using state_type = ::fast_io::details::scan_context_state_t<char_type, scanner_type>;
	// Keep the ordinary small-state path direct. A generic callback wrapper caused GCC to outline only the context-only
	// scanner's lambda while fully inlining an otherwise identical hybrid scanner, adding one call per scanned object.
	// Encoding the stack policy in this specialization preserves the same cross-configuration ODR proof as the shared
	// owner. The large branch still contains no automatic `state_type`; it enters the isolated noinline allocation helper.
	if constexpr (::fast_io::details::scan_context_state_inline_v<state_type, stack_policy>)
	{
		state_type state{};
		return ::fast_io::details::scan_context_status_with_state(in, arg, state);
	}
	else
	{
		return ::fast_io::details::with_large_scan_context_state<state_type>([&](state_type &state) {
			return ::fast_io::details::scan_context_status_with_state(in, arg, state);
		});
	}
}

/// @brief Attempts one explicitly transactional scan on the currently available buffered chunk.
/// @return `true` only when the value completed. A `false` result is an exact no-progress miss and instructs the caller
///         to enter the unchanged context state machine. Decisive parse errors are committed and thrown here.
template <typename input, typename T>
	requires ::fast_io::current_chunk_context_scannable<
		typename input::input_char_type, T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_context_current_chunk_try(input &in, T &arg)
{
	using char_type = typename input::input_char_type;
	using scanner_type = ::std::remove_cvref_t<T>;
	auto const current_pointer{ibuffer_curr(in)};
	auto const end_pointer{ibuffer_end(in)};
	if (current_pointer == end_pointer)
	{
		return false;
	}
	auto const first{static_cast<char_type const *>(current_pointer)};
	auto const last{static_cast<char_type const *>(end_pointer)};
	auto [it, ec] = scan_context_current_chunk_define(
		io_reserve_type<char_type, scanner_type>, first, last, arg);
	if (ec == parse_code::partial)
	{
		// A miss is transactional: neither cursor nor target may have changed. Requiring the original iterator makes the
		// cursor half of that contract mechanically checkable before the context fallback is entered.
		if (it != first) [[unlikely]]
		{
			throw_parse_code(parse_code::invalid);
		}
		return false;
	}
	if (it == last || ec == parse_code::end_of_file) [[unlikely]]
	{
		// This protocol never assigns EOF meaning to the chunk end. A decisive result must leave at least one character
		// proving that the token ended inside the supplied chunk.
		throw_parse_code(parse_code::invalid);
	}
	if constexpr (::fast_io::current_chunk_context_scanner_result_in_range<char_type, T>)
	{
		::fast_io::details::scan_commit_bounded_iterator(in, current_pointer, first, it);
	}
	else if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(
				 in, current_pointer, first, last, it)) [[unlikely]]
	{
		throw_parse_code(parse_code::invalid);
	}
	if (ec == parse_code::ok)
	{
		return true;
	}
	throw_parse_code(ec);
}

template <typename input, typename T>
inline constexpr bool scan_context_current_chunk_dispatch_available_impl() noexcept
{
	using char_type = typename input::input_char_type;
	using scanner_type = ::std::remove_cvref_t<T>;
	if constexpr (
		!::fast_io::current_chunk_context_scannable<char_type, T> ||
		!::fast_io::operations::decay::defines::has_ibuffer_minimum_size_operations<input>)
	{
		return false;
	}
	else
	{
		return ibuffer_minimum_size_define(
				   ::fast_io::io_reserve_type<char_type, input>) >=
			   scan_context_current_chunk_minimum_size(
				   ::fast_io::io_reserve_type<char_type, scanner_type>);
	}
}

template <typename input, typename T>
inline constexpr bool scan_context_current_chunk_dispatch_available{
	scan_context_current_chunk_dispatch_available_impl<input, T>()};

// A current-chunk accelerator misses only when a token reaches a chunk boundary (or before the first refill). Keep the
// complete context state machine out of the scalar fast-path frame: inlining it forces every successful scan to carry
// its state storage, saved registers, and stack-protector epilogue even though none of that work is used. The separate
// cold owner preserves the transactional fallback semantics while leaving the ordinary in-chunk path small enough for
// both GCC and Clang to optimize as an independent strategy.
template <typename stack_policy, typename input, typename T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline bool scan_context_current_chunk_fallback(input &in, T &arg)
{
	return ::fast_io::details::scan_context_status_impl<stack_policy>(in, arg);
}

template <bool>
inline constexpr bool type_not_scannable = false;

template <typename input, typename T>
inline constexpr bool scan_terminal_padding_dispatch_available = [] {
	using char_type = typename input::input_char_type;
	if constexpr (
		!::fast_io::contiguous_range_with_padding<input> ||
		!::fast_io::terminal_contiguous_padding_scannable<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::context_scannable<char_type, T>)
	{
		return ::fast_io::terminal_padding_context_scannable<
			char_type, T>;
	}
	else
	{
		return true;
	}
}();

/*
Keep the ordinary scalar dispatcher as a separate constrained specialization.
Its body is the pre-padding implementation: it neither recognizes nor names a
padding protocol.  This separation is deliberate.  Even a discarded
`if constexpr` inside this very hot function changed GCC's inlining budget for
ordinary integer scans, despite producing no padding operation at run time.
*/
template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename T>
	requires(
		!::fast_io::details::
			scan_terminal_padding_dispatch_available<input, T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr bool scan_single_impl(input &in, T &arg)
{
	// Precise staging may be reached only after the direct current-chunk test fails (or unconditionally under ASan).
	// Carrying the policy in this caller's identity prevents inlining that policy-dependent branch into an otherwise
	// identically named scalar dispatcher. Context storage uses the same policy for the corresponding inline-state proof.
	using char_type = typename input::input_char_type;
	{
		using scanner_type = ::std::remove_cvref_t<T>;
		if constexpr (precise_reserve_scannable<char_type, T>)
		{
			constexpr ::std::size_t n{
				scan_precise_reserve_size(io_reserve_type<char_type, scanner_type>)};
			if constexpr (::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate)
			{
				// Sanitizer mode deliberately prevents a scanner from observing spare ibuffer capacity beyond its exact
				// protocol extent. The common staging helper also preserves report-mode EOF and large-buffer policy.
				return ::fast_io::details::scan_precise_reserve_staging<n, stack_policy>(in, arg);
			}
			else
			{
				if constexpr (n == 0u)
				{
					return ::fast_io::details::scan_precise_reserve_staging<0u, stack_policy>(in, arg);
				}
				auto curr_ptr{ibuffer_curr(in)};
				auto end_ptr{ibuffer_end(in)};
				if (curr_ptr == end_ptr)
				{
					if (!ibuffer_underflow(in)) [[unlikely]]
					{
						return false;
					}
					curr_ptr = ibuffer_curr(in);
					end_ptr = ibuffer_end(in);
					if (curr_ptr == end_ptr) [[unlikely]]
					{
						throw_parse_code(parse_code::invalid);
					}
				}
				char_type const *curr{curr_ptr};
				char_type const *end{end_ptr};
				::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
				if (diff >= n) [[likely]]
				{
					// Fixed-extent input is consumed before validation. The staging path must consume while collecting data
					// across refills and cannot generically rewind an underlying device; committing here gives direct,
					// boundary, and sanitizer strategies one observable cursor rule.
					ibuffer_set_curr(in, curr_ptr + n);
					return ::fast_io::details::scan_precise_reserve_apply<char_type>(curr, arg);
				}
				return ::fast_io::details::scan_precise_reserve_staging<n, stack_policy>(in, arg);
			}
		}
		else if constexpr (context_scannable<char_type, T>)
		{
			if constexpr (terminal_contiguous_context_scannable<char_type, T> &&
						  ::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>)
			{
				// Capability presence is not a terminal proof: one observer type may report either mode at run time.
				// Only the true branch may consume the semantic-equivalence promise made by the scanner marker.
				if (ibuffer_underflow_never(in)) [[likely]]
				{
					auto curr{ibuffer_curr(in)};
					auto end{ibuffer_end(in)};
					auto [it, ec] = scan_contiguous_define(
						io_reserve_type<char_type, scanner_type>, curr, end, arg);
					if constexpr (::fast_io::contiguous_scanner_result_in_range<char_type, T>)
					{
						::fast_io::details::scan_commit_bounded_iterator(
							in, curr, static_cast<char_type const *>(curr), it);
					}
					else if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(
								 in, curr, static_cast<char_type const *>(curr),
								 static_cast<char_type const *>(end), it)) [[unlikely]]
					{
						throw_parse_code(parse_code::invalid);
					}
					if (ec == parse_code::ok)
					{
						return true;
					}
					if (ec == parse_code::end_of_file || ec == parse_code::partial)
					{
						// `partial` has no distinct public meaning on a terminal buffer: no refill can complete it.
						return false;
					}
					throw_parse_code(ec);
				}
			}
			if constexpr (::fast_io::details::scan_context_current_chunk_dispatch_available<input, T>)
			{
				if (::fast_io::details::scan_context_current_chunk_try(in, arg)) [[likely]]
				{
					return true;
				}
				return ::fast_io::details::scan_context_current_chunk_fallback<stack_policy>(in, arg);
			}
			// Context-only scanners preserve the ordinary stateful path exactly.
			return ::fast_io::details::scan_context_status_impl<stack_policy>(in, arg);
		}
		else if constexpr (::fast_io::contiguous_padding_scannable_protocol<char_type, T> &&
					   !::fast_io::contiguous_scannable<char_type, T>)
		{
			// A padding-only scanner is terminal by construction: there is no
			// refill-safe ordinary CPO to use on a live chunk.  Require the same
			// terminal ibuffer proof as the ordinary contiguous-only branch and let
			// the shared status helper select the new or legacy padding spelling.
			static_assert(
				::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>,
				"a padding-only scanner requires a terminal ibuffer");
			if (!ibuffer_underflow_never(in)) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			return ::fast_io::details::scan_contiguous_status_impl(in, arg);
		}
		else if constexpr (contiguous_scannable<char_type, T>)
		{
			static_assert(
				::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>,
				"A contiguous-only scanner requires a terminal ibuffer; refillable input needs a context scanner.");
			if (!ibuffer_underflow_never(in)) [[unlikely]]
			{
				// The type supplies the query but this object is currently refillable. Without a context protocol there is
				// no semantics-preserving fallback, so treating the current chunk as a complete token would be incorrect.
				throw_parse_code(parse_code::invalid);
			}
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			// Empty spans remain semantic input. A zero-width scanner may succeed without consuming a character, and a
			// valid terminal view supplies an equal pointer pair even when its length is zero.
			auto [it, ec] = scan_contiguous_define(
				io_reserve_type<char_type, scanner_type>, curr, end, arg);
			if constexpr (::fast_io::contiguous_scanner_result_in_range<char_type, T>)
			{
				::fast_io::details::scan_commit_bounded_iterator(
					in, curr, static_cast<char_type const *>(curr), it);
			}
			else if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(
						 in, curr, static_cast<char_type const *>(curr),
						 static_cast<char_type const *>(end), it)) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			if (ec == parse_code::ok)
			{
				return true;
			}
			if (ec == parse_code::end_of_file || ec == parse_code::partial)
			{
				return false;
			}
			throw_parse_code(ec);
		}
		else
		{
			constexpr bool not_scannable{context_scannable<char_type, T>};
			static_assert(not_scannable, "type not scannable. need context_scannable");
			return false;
		}
	}
}

template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename T>
	requires ::fast_io::details::
		scan_terminal_padding_dispatch_available<input, T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr bool scan_single_impl(input &in, T &arg)
{
	// Precise staging may be reached only after the direct current-chunk test fails (or unconditionally under ASan).
	// Carrying the policy in this caller's identity prevents inlining that policy-dependent branch into an otherwise
	// identically named scalar dispatcher. Context storage uses the same policy for the corresponding inline-state proof.
	using char_type = typename input::input_char_type;
#if 0
	if constexpr(contiguous_input_stream<input>)
	{
		if constexpr(precise_reserve_scannable<char_type,T>)
		{
			constexpr ::std::size_t n{scan_precise_reserve_size(io_reserve_type<char_type,T>)};
			auto curr_ptr{ibuffer_curr(in)};
			char_type const* curr{curr_ptr};
			char_type const* end{ibuffer_end(in)};
			::std::size_t const diff{static_cast<::std::size_t>(end-curr)};
			if(diff<n)[[unlikely]]
			{
				return false;
			}
			if constexpr(precise_reserve_scannable_no_error<char_type,T>)
			{
				scan_precise_reserve_define(io_reserve_type<char_type,T>,curr,arg);
			}
			else
			{
				auto ret{scan_precise_reserve_define(io_reserve_type<char_type,T>,curr,arg)};
				if(ret!=parse_code::ok)
				{
					if(ret==parse_code::end_of_file)
					{
						return false;
					}
					throw_parse_code(ret);
				}
			}
			ibuffer_set_curr(in,curr_ptr+n);
			return true;
		}
		else if constexpr(contiguous_scannable<char_type,T>)
		{
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			auto [it,ec] = scan_contiguous_define(io_reserve_type<char_type,T>,curr,end,arg);
			if constexpr(::std::same_as<decltype(curr),decltype(it)>)
			{
				ibuffer_set_curr(in,it);
			}
			else
			{
				ibuffer_set_curr(in,it-curr+curr);
			}
			if(ec!=parse_code::ok)
			{
				if(ec==parse_code::end_of_file)
					return false;
				throw_parse_code(ec);
			}
			return true;
		}
		else if constexpr(context_scannable<char_type,T>)
		{
			typename ::std::remove_cvref_t<decltype(scan_context_type(io_reserve_type<char_type,T>))>::type state;
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			auto [it,ec]=scan_context_define(io_reserve_type<char_type,T>,state,curr,end,arg);
			if constexpr(::std::same_as<decltype(curr),decltype(it)>)
			{
				ibuffer_set_curr(in,it);
			}
			else
			{
				ibuffer_set_curr(in,it-curr+curr);
			}
			if(ec==parse_code::ok)
				return true;
			else if(ec!=parse_code::partial)
				throw_parse_code(ec);
			ec=scan_context_eof_define(io_reserve_type<char_type,T>,state,arg);
			if(ec==parse_code::ok)
				return true;
			else if(ec!=parse_code::end_of_file)
				throw_parse_code(ec);
			return false;
		}
		else
		{
			constexpr bool not_scannable{context_scannable<char_type,T>};
			static_assert(not_scannable,"type not scannable. need context_scannable");
			return false;
		}
	}
	else
#endif
	{
		using scanner_type = ::std::remove_cvref_t<T>;
		if constexpr (precise_reserve_scannable<char_type, T>)
		{
			constexpr ::std::size_t n{
				scan_precise_reserve_size(io_reserve_type<char_type, scanner_type>)};
			if constexpr (::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate)
			{
				// Sanitizer mode deliberately prevents a scanner from observing spare ibuffer capacity beyond its exact
				// protocol extent. The common staging helper also preserves report-mode EOF and large-buffer policy.
				return ::fast_io::details::scan_precise_reserve_staging<n, stack_policy>(in, arg);
			}
			else
			{
				if constexpr (n == 0u)
				{
					return ::fast_io::details::scan_precise_reserve_staging<0u, stack_policy>(in, arg);
				}
				auto curr_ptr{ibuffer_curr(in)};
				auto end_ptr{ibuffer_end(in)};
				if (curr_ptr == end_ptr)
				{
					if (!ibuffer_underflow(in)) [[unlikely]]
					{
						return false;
					}
					curr_ptr = ibuffer_curr(in);
					end_ptr = ibuffer_end(in);
					if (curr_ptr == end_ptr) [[unlikely]]
					{
						throw_parse_code(parse_code::invalid);
					}
				}
				char_type const *curr{curr_ptr};
				char_type const *end{end_ptr};
				::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
				if (diff >= n) [[likely]]
				{
					// Fixed-extent input is consumed before validation. The staging path must consume while collecting data
					// across refills and cannot generically rewind an underlying device; committing here gives direct,
					// boundary, and sanitizer strategies one observable cursor rule.
					ibuffer_set_curr(in, curr_ptr + n);
					return ::fast_io::details::scan_precise_reserve_apply<char_type>(curr, arg);
				}
				return ::fast_io::details::scan_precise_reserve_staging<n, stack_policy>(in, arg);
			}
		}
		else if constexpr (context_scannable<char_type, T>)
		{
			if constexpr (terminal_padding_context_scannable<char_type, T> &&
						  ::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>)
			{
				// Capability presence is not a terminal proof: one observer type may report either mode at run time.
				// Only the true branch may consume the semantic-equivalence promise made by the scanner marker.
				if (ibuffer_underflow_never(in)) [[likely]]
				{
					if constexpr (
						::fast_io::contiguous_range_with_padding<input> &&
						::fast_io::terminal_contiguous_padding_scannable<
							char_type, T>)
					{
						return ::fast_io::details::
							scan_contiguous_status_impl(in, arg);
					}
					else
					{
						auto curr{ibuffer_curr(in)};
						auto end{ibuffer_end(in)};
						auto [it, ec] = ::fast_io::details::scan_contiguous_invoke(
							in, static_cast<char_type const *>(curr),
							static_cast<char_type const *>(end), arg);
						if (!::fast_io::details::
								scan_commit_iterator_if_in_current_chunk(
									in, curr,
									static_cast<char_type const *>(curr),
									static_cast<char_type const *>(end),
									it)) [[unlikely]]
						{
							throw_parse_code(parse_code::invalid);
						}
						if (ec == parse_code::ok)
						{
							return true;
						}
						if (ec == parse_code::end_of_file ||
							ec == parse_code::partial)
						{
							// `partial` has no distinct public meaning on a terminal buffer: no refill can complete it.
							return false;
						}
						throw_parse_code(ec);
					}
				}
			}
			if constexpr (::fast_io::details::scan_context_current_chunk_dispatch_available<input, T>)
			{
				if (::fast_io::details::scan_context_current_chunk_try(in, arg)) [[likely]]
				{
					return true;
				}
			}
			return ::fast_io::details::scan_context_status_impl<stack_policy>(in, arg);
		}
		else if constexpr (::fast_io::contiguous_padding_scannable_protocol<char_type, T> &&
					   !::fast_io::contiguous_scannable<char_type, T>)
		{
			// Padding-only scanners are terminal by construction.  This overload
			// is selected when the input exposes a padding range, so route it
			// through the shared bounded-cursor helper instead of falling into the
			// ordinary scanner diagnostic below.
			static_assert(
				::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>,
				"a padding-only scanner requires a terminal ibuffer");
			if (!ibuffer_underflow_never(in)) [[unlikely]]
			{
				throw_parse_code(parse_code::invalid);
			}
			return ::fast_io::details::scan_contiguous_status_impl(in, arg);
		}
		else if constexpr (contiguous_scannable<char_type, T>)
		{
			static_assert(
				::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<input>,
				"A contiguous-only scanner requires a terminal ibuffer; refillable input needs a context scanner.");
			if (!ibuffer_underflow_never(in)) [[unlikely]]
			{
				// The type supplies the query but this object is currently refillable. Without a context protocol there is
				// no semantics-preserving fallback, so treating the current chunk as a complete token would be incorrect.
				throw_parse_code(parse_code::invalid);
			}
			if constexpr (
				::fast_io::contiguous_range_with_padding<input> &&
				::fast_io::terminal_contiguous_padding_scannable<
					char_type, T>)
			{
				return ::fast_io::details::
					scan_contiguous_status_impl(in, arg);
			}
			else
			{
				auto curr{ibuffer_curr(in)};
				auto end{ibuffer_end(in)};
				// Empty spans remain semantic input. A zero-width scanner may succeed without consuming a character, and a
				// valid terminal view supplies an equal pointer pair even when its length is zero.
				auto [it, ec] = scan_contiguous_define(
					io_reserve_type<char_type, scanner_type>,
					curr, end, arg);
				if (!::fast_io::details::
						scan_commit_iterator_if_in_current_chunk(
							in, curr,
							static_cast<char_type const *>(curr),
							static_cast<char_type const *>(end),
							it)) [[unlikely]]
				{
					throw_parse_code(parse_code::invalid);
				}
				if (ec == parse_code::ok)
				{
					return true;
				}
				if (ec == parse_code::end_of_file ||
					ec == parse_code::partial)
				{
					return false;
				}
				throw_parse_code(ec);
			}
		}
		else
		{
			constexpr bool not_scannable{context_scannable<char_type, T>};
			static_assert(not_scannable, "type not scannable. need context_scannable");
			return false;
		}
	}
}

namespace decay
{

/// @brief Selects how normalized proxy values cross the outlined scalar-fallback boundary.
/// @details `whole_value` is coupled to the separately bounded entry owner: every controller argument already denotes
///          owned storage and the complete small pack may cross one bounded by-value transport boundary.
///          `chunked_value` is different: its entry keeps exact references, while only a cold/refill branch copies
///          semantically transport-safe proxy
///          values into bounded calls. Keeping these states distinct prevents the valid whole-entry count ceiling from
///          forcing large marked packs onto an address-escaping reference fallback. The reference state remains the
///          universal protocol path for identity-sensitive, noncopyable, cv-qualified, or unmarked proxies.
enum class scan_proxy_fallback_transport : unsigned char
{
	reference,
	whole_value,
	chunked_value
};

struct precise_scan_run_result
{
	::std::size_t position{};
	::std::size_t neededspace{};
	bool aggregate_commit_safe{};
};

/// @brief Admits exact-void precise scanners to a shared aggregate availability check.
/// @details Exception safety is supplied by the execution policy: an unmarked run publishes each scalar cursor before
///          its CPO. Consequently this recognition need not reject a potentially throwing customization.
template <::std::integral char_type, typename T>
inline constexpr bool batch_precise_scannable_no_error =
	::fast_io::precise_reserve_scannable_no_error<char_type, T>;

template <::std::integral char_type, typename T>
inline constexpr bool batch_precise_aggregate_commit_safe = [] {
	if constexpr (!::fast_io::aggregate_commit_safe_precise_reserve_scannable<char_type, T>)
	{
		return false;
	}
	else
	{
		// Sharing one availability check does not require a nothrow producer: the default batch body publishes each
		// scalar cursor before invoking its CPO and therefore preserves the scalar exception boundary exactly. Delaying
		// all publications until the end is stronger. It requires both the semantic marker and `noexcept`, so an
		// exception can neither expose an old cursor nor leave an applied prefix behind that cursor.
		return noexcept(scan_precise_reserve_define(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
			static_cast<char_type const *>(nullptr), ::std::declval<T &>()));
	}
}();

template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr precise_scan_run_result find_continuous_precise_scan_n() noexcept
{
	if constexpr (::fast_io::details::decay::batch_precise_scannable_no_error<char_type, Arg>)
	{
		constexpr ::std::size_t sz{scan_precise_reserve_size(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<Arg>>)};
		if constexpr (sizeof...(Args) == 0)
		{
			return {1u, sz,
					::fast_io::details::decay::batch_precise_aggregate_commit_safe<char_type, Arg>};
		}
		else
		{
			constexpr precise_scan_run_result res{
				::fast_io::details::decay::find_continuous_precise_scan_n<char_type, Args...>()};
			constexpr ::std::size_t maximum_pointer_distance{
				static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
			if constexpr (res.position == 0u)
			{
				// The following target terminates this precise run. Its default `false` policy is not a member of the run
				// and must not poison the current target's aggregate-commit proof. This boundary matters for a long marked
				// precise prefix followed by one context scanner: propagating the sentinel bit silently changed one commit
				// into N scalar commits even though every target actually in the prefix supplied the marker.
				return {1u, sz,
						::fast_io::details::decay::batch_precise_aggregate_commit_safe<char_type, Arg>};
			}
			else if constexpr (maximum_pointer_distance - sz < res.neededspace)
			{
				// Batching is only an optimization. Each scanner has already proved that its own fixed extent belongs to
				// the pointer-difference domain, but their aggregate need not. Rejecting the complete scanner pack (or
				// terminating during constant evaluation) would incorrectly turn an optimization capacity into a protocol
				// precondition. Keep the current scanner as a one-element run; scalar dispatch consumes it, then recursion
				// is free to batch the remaining suffix independently.
				return {1u, sz,
						::fast_io::details::decay::batch_precise_aggregate_commit_safe<char_type, Arg>};
			}
			else
			{
				return {
					res.position + 1u, res.neededspace + sz,
					res.aggregate_commit_safe &&
						::fast_io::details::decay::batch_precise_aggregate_commit_safe<char_type, Arg>};
			}
		}
	}
	else
	{
		return {};
	}
}

template <::std::integral char_type, typename tuple_type, ::std::size_t... indices>
inline constexpr char_type const *scan_precise_tuple_no_error(
	char_type const *current, tuple_type &targets, ::std::index_sequence<indices...>)
{
	// One fold gives the optimizer a single linear expression. The old recursive helper carried the complete remaining
	// parameter list in every specialization; once inlining stopped, a 256-target run left several large ABI shims and
	// superlinear text. Target order remains the language-defined left-to-right order of the comma fold.
	auto apply_one = [&](auto &target) {
		using scanner_type = ::std::remove_cvref_t<decltype(target)>;
		constexpr ::std::size_t sz{
			scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, scanner_type>)};
		scan_precise_reserve_define(
			::fast_io::io_reserve_type<char_type, scanner_type>, current, target);
		current += sz;
	};
	(apply_one(::fast_io::containers::get<indices>(targets)), ...);
	return current;
}

template <::std::size_t n, ::std::integral char_type, typename... Args>
inline constexpr char_type const *scan_n_precise_reserve_no_error(char_type const *p, Args &...args)
{
	static_assert(n <= sizeof...(Args));
	if constexpr (n == sizeof...(Args))
	{
		// The overwhelmingly common batch is the complete remaining pack. Expanding it directly avoids constructing a
		// tuple of hundreds of reference members solely to recover every member immediately; prefix runs retain the
		// indexed tuple path below because C++20 has no native pack slice.
		auto apply_one = [&](auto &target) {
			using scanner_type = ::std::remove_cvref_t<decltype(target)>;
			constexpr ::std::size_t sz{
				scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, scanner_type>)};
			scan_precise_reserve_define(
				::fast_io::io_reserve_type<char_type, scanner_type>, p, target);
			p += sz;
		};
		(apply_one(args), ...);
		return p;
	}
	else
	{
		auto targets{::fast_io::containers::forward_as_tuple(args...)};
		return ::fast_io::details::decay::scan_precise_tuple_no_error<char_type>(
			p, targets, ::std::make_index_sequence<n>{});
	}
}

/// @brief Applies a prevalidated precise run while publishing the scalar cursor schedule.
/// @details One aggregate availability check proves every pointer below. Each positive extent is committed before its
///          CPO, exactly like `scan_single_impl`, so a scanner that indirectly observes the input sees the cursor after
///          its own record and before the following record. The scanner protocol does not grant a target permission to
///          mutate its source stream indirectly; doing so would invalidate both scalar and batched ownership rules.
template <::std::integral char_type, typename input, typename current_pointer,
		  typename tuple_type, ::std::size_t... indices>
inline constexpr void scan_precise_tuple_with_scalar_commits(
	input &in, current_pointer current, char_type const *p, tuple_type &targets,
	::std::index_sequence<indices...>)
{
	auto apply_one = [&](auto &target) {
		using scanner_type = ::std::remove_cvref_t<decltype(target)>;
		constexpr ::std::size_t sz{
			scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, scanner_type>)};
		if constexpr (sz != 0u)
		{
			current += sz;
			ibuffer_set_curr(in, current);
		}
		scan_precise_reserve_define(::fast_io::io_reserve_type<char_type, scanner_type>, p, target);
		p += sz;
	};
	(apply_one(::fast_io::containers::get<indices>(targets)), ...);
}

template <::std::size_t n, ::std::integral char_type, typename input, typename current_pointer,
		  typename... Args>
inline constexpr void scan_n_precise_reserve_with_scalar_commits(
	input &in, current_pointer current, char_type const *p, Args &...args)
{
	static_assert(n <= sizeof...(Args));
	if constexpr (n == sizeof...(Args))
	{
		auto apply_one = [&](auto &target) {
			using scanner_type = ::std::remove_cvref_t<decltype(target)>;
			constexpr ::std::size_t sz{
				scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, scanner_type>)};
			if constexpr (sz != 0u)
			{
				current += sz;
				ibuffer_set_curr(in, current);
			}
			scan_precise_reserve_define(::fast_io::io_reserve_type<char_type, scanner_type>, p, target);
			p += sz;
		};
		(apply_one(args), ...);
	}
	else
	{
		auto targets{::fast_io::containers::forward_as_tuple(args...)};
		::fast_io::details::decay::scan_precise_tuple_with_scalar_commits<char_type>(
			in, current, p, targets, ::std::make_index_sequence<n>{});
	}
}

/// @brief Executes the correctness fallback for a pack that is not already contiguous as one run.
/// @details Keeping this scalar controller separate prevents every suffix from embedding another complete batched
///          fast path. The former nested fallback produced quadratic text growth (visible at 64 and 256 arguments)
///          even though only the outer fast path was executed. One non-inline boundary retains a linear fallback body
///          without placing its full refill/state machinery in the hot contiguous branch.
template <typename stack_policy, typename input, typename tuple_type, ::std::size_t... indices>
inline constexpr bool scan_controls_scalar_tuple(
	input &in, tuple_type &targets, ::std::index_sequence<indices...>)
{
	// Built-in `&&` folds sequence left-to-right and stop at the first reported EOF, preserving the public pack rule
	// without recursively instantiating one complete controller for every suffix.
	return (::fast_io::details::scan_single_impl<stack_policy>(
				in, ::fast_io::containers::get<indices>(targets)) &&
			...);
}

template <typename stack_policy, typename input, typename... Args>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback(input &in, Args &...args)
{
	auto targets{::fast_io::containers::forward_as_tuple(args...)};
	return ::fast_io::details::decay::scan_controls_scalar_tuple<stack_policy>(
		in, targets, ::std::index_sequence_for<Args...>{});
}

/// @brief Outlined scalar fallback for one bounded pack of semantically transport-safe proxy values.
/// @details The reference fallback above forces every proxy address to escape to a no-inline call. Even when the
///          aggregate-buffer branch is taken, GCC must then materialize those addresses before the bounds check. This
///          separate by-value overload lets the compiler keep proxy fields in SSA form on the hot branch and emit call
///          setup only after the fallback branch is selected. It is never instantiated merely because a type is
///          trivial: public dispatch also requires the semantic transport marker and strict object/call-shape budgets.
///          Large packs do not call this function with an unbounded argument list; the always-inline chunk bridge below
///          partitions them first. The reference overload remains necessary for noncopyable and identity-sensitive
///          aliases.
template <typename stack_policy, typename input, typename... Args>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback_owned(input &in, Args... args)
{
	auto targets{::fast_io::containers::forward_as_tuple(args...)};
	return ::fast_io::details::decay::scan_controls_scalar_tuple<stack_policy>(
		in, targets, ::std::index_sequence_for<Args...>{});
}

/// @brief Copies one bounded slice into the only no-inline function used by the large value fallback.
/// @details This bridge must remain inline. Passing its reference tuple across an outlined boundary would make every
///          original alias temporary address escape and would recreate the 64-argument hot-loop regression this policy
///          exists to remove. Only the selected fields cross the actual call boundary, by value. The fixed worst-case
///          chunk count proves both the existing object-count limit and the rounded-byte limit without inspecting or
///          recursively summing the complete pack.
template <::std::size_t offset, typename stack_policy, typename input,
		  typename tuple_type, ::std::size_t... indices>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback_value_chunk(
	input &in, tuple_type &targets, ::std::index_sequence<indices...>)
{
	static_assert(sizeof...(indices) != 0u);
	static_assert(sizeof...(indices) <=
				  ::fast_io::operations::decay::scan_proxy_value_fallback_chunk_max_count);
	return ::fast_io::details::decay::scan_controls_scalar_fallback_owned<stack_policy>(
		in, ::fast_io::containers::get<offset + indices>(targets)...);
}

/// @brief Applies a large value fallback as a linear sequence of bounded calls.
/// @details Each instantiation advances by one fixed-size slice over the same tuple type; it never forms a controller
///          for every remaining suffix. Consequently both the number of helper calls and template work are O(N), while
///          each emitted call has O(1) argument count and rounded bytes under the measured budget. Built-in `&&`
///          semantics are expressed explicitly:
///          EOF stops before the next slice, and an exception propagates before any later slice is formed. Values later
///          in the current call have already been trivially copied, but the semantic marker proves those copies have no
///          observable effect; no later scanner CPO runs.
template <::std::size_t offset, ::std::size_t total, typename stack_policy,
		  typename input, typename tuple_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback_value_chunks_impl(
	input &in, tuple_type &targets)
{
	static_assert(offset < total);
	constexpr ::std::size_t remaining{total - offset};
	constexpr ::std::size_t chunk_count{
		remaining < ::fast_io::operations::decay::scan_proxy_value_fallback_chunk_max_count
			? remaining
			: ::fast_io::operations::decay::scan_proxy_value_fallback_chunk_max_count};
	if (!::fast_io::details::decay::scan_controls_scalar_fallback_value_chunk<
			offset, stack_policy>(in, targets, ::std::make_index_sequence<chunk_count>{}))
	{
		return false;
	}
	if constexpr (chunk_count == remaining)
	{
		return true;
	}
	else
	{
		return ::fast_io::details::decay::scan_controls_scalar_fallback_value_chunks_impl<
			offset + chunk_count, total, stack_policy>(in, targets);
	}
}

template <typename stack_policy, typename input, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback_value_chunks(input &in, Args &...args)
{
	static_assert(sizeof...(Args) != 0u);
	auto targets{::fast_io::containers::forward_as_tuple(args...)};
	return ::fast_io::details::decay::scan_controls_scalar_fallback_value_chunks_impl<
		0u, sizeof...(Args), stack_policy>(in, targets);
}

/// @brief Preserves one compile-time transport decision through every controller suffix.
/// @details Recomputing from named `args` would always see lvalues and silently disable semantic value transport;
///          recomputing after mutex recursion has the same problem. Public dispatch therefore selects this state once
///          from the normalized expression categories. The chunk bridges and this selector are forced inline so the
///          hot controller never passes `Args&...` to a no-inline fallback in either value-transport specialization.
template <scan_proxy_fallback_transport transport, typename stack_policy,
		  typename input, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool scan_controls_scalar_fallback_select(input &in, Args &...args)
{
	// A short scalar pack is smaller than the outlined ABI bridge and benefits from sharing the terminal/current cursor
	// directly across adjacent leaf scans.  Larger packs keep the bounded outline: it prevents the refill/context bodies
	// and proxy-address setup from growing with every call site, which is the code-size case this controller was added for.
	if constexpr (sizeof...(Args) <= 4u)
	{
		auto targets{::fast_io::containers::forward_as_tuple(args...)};
		return ::fast_io::details::decay::scan_controls_scalar_tuple<stack_policy>(
			in, targets, ::std::index_sequence_for<Args...>{});
	}
	else if constexpr (transport == scan_proxy_fallback_transport::whole_value)
	{
		return ::fast_io::details::decay::scan_controls_scalar_fallback_owned<stack_policy>(
			in, args...);
	}
	else if constexpr (transport == scan_proxy_fallback_transport::chunked_value)
	{
		return ::fast_io::details::decay::scan_controls_scalar_fallback_value_chunks<stack_policy>(
			in, args...);
	}
	else
	{
		return ::fast_io::details::decay::scan_controls_scalar_fallback<stack_policy>(in, args...);
	}
}

template <typename stack_policy, scan_proxy_fallback_transport transport,
		  typename input, typename T, typename... Args>
[[nodiscard]] inline constexpr bool scan_controls_impl(input &in, T &t, Args &...args);

template <::std::size_t processed, typename stack_policy, scan_proxy_fallback_transport transport,
		  typename input, typename tuple_type, ::std::size_t... indices>
[[nodiscard]] inline constexpr bool scan_controls_suffix_tuple(
	input &in, tuple_type &targets, ::std::index_sequence<indices...>)
{
	return ::fast_io::details::decay::scan_controls_impl<stack_policy, transport>(
		in, ::fast_io::containers::get<processed + indices>(targets)...);
}

template <::std::size_t processed, typename stack_policy, scan_proxy_fallback_transport transport,
		  typename input, typename... Args>
[[nodiscard]] inline constexpr bool scan_controls_suffix(input &in, Args &...args)
{
	static_assert(processed < sizeof...(Args));
	auto targets{::fast_io::containers::forward_as_tuple(args...)};
	return ::fast_io::details::decay::scan_controls_suffix_tuple<
		processed, stack_policy, transport>(
		in, targets, ::std::make_index_sequence<sizeof...(Args) - processed>{});
}

template <typename stack_policy, scan_proxy_fallback_transport transport, typename input>
[[nodiscard]] inline constexpr bool scan_controls_impl(input &) noexcept
{
	return true;
}

template <typename stack_policy, scan_proxy_fallback_transport transport,
		  typename input, typename T, typename... Args>
[[nodiscard]] inline constexpr bool scan_controls_impl(input &in, T &t, Args &...args)
{
	if constexpr (::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate)
	{
		return ::fast_io::details::decay::scan_controls_scalar_fallback_select<
			transport, stack_policy>(in, t, args...);
	}
	else
	{
		using char_type = typename input::input_char_type;
		constexpr precise_scan_run_result res{
			::fast_io::details::decay::find_continuous_precise_scan_n<char_type, T, Args...>()};
		if constexpr (res.position > 1u && res.neededspace == 0u)
		{
			// A zero-width run needs no stream observation or refill. One real object supplies a valid empty-range
			// address to every CPO and removes the old O(N) scalar dispatch chain for semantic initialization packs.
			char_type dummy{};
			::fast_io::details::decay::scan_n_precise_reserve_no_error<res.position, char_type>(
				__builtin_addressof(dummy), t, args...);
			if constexpr (res.position == sizeof...(Args) + 1u)
			{
				return true;
			}
			else
			{
				return ::fast_io::details::decay::scan_controls_suffix<
					res.position, stack_policy, transport>(
					in, t, args...);
			}
		}
		else if constexpr (res.position > 1u)
		{
			auto curr_ptr{ibuffer_curr(in)};
			auto end_ptr{ibuffer_end(in)};
			// A newly constructed dynamic ibuffer may expose two null cursors. Defer to the single-item path, which
			// performs the first underflow before subtracting pointers, rather than forming nullptr - nullptr here.
			if (curr_ptr != end_ptr)
			{
				char_type const *curr{curr_ptr};
				char_type const *end{end_ptr};
				::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
				if (diff >= res.neededspace) [[likely]]
				{
					if constexpr (res.aggregate_commit_safe)
					{
						::fast_io::details::decay::scan_n_precise_reserve_no_error<res.position, char_type>(
							curr, t, args...);
						ibuffer_set_curr(in, curr_ptr + res.neededspace);
					}
					else
					{
						::fast_io::details::decay::scan_n_precise_reserve_with_scalar_commits<
							res.position, char_type>(in, curr_ptr, curr, t, args...);
					}
					if constexpr (res.position == sizeof...(Args) + 1u)
					{
						return true;
					}
					else
					{
						return ::fast_io::details::decay::scan_controls_suffix<
							res.position, stack_policy, transport>(in, t, args...);
					}
				}
			}
			// The run did not fit the current chunk. Enter one linear scalar/refill implementation instead of embedding
			// another batched controller for every suffix, which was the source of the large-pack text-size accident.
			return ::fast_io::details::decay::scan_controls_scalar_fallback_select<
				transport, stack_policy>(in, t, args...);
		}

		// Once the leading item is not part of a selected precise run, one linear scalar fold preserves order and
		// reporting semantics. Re-entering the full batch detector for every remaining suffix was another quadratic
		// template/code-size path for large context or alternating-capability packs.
		return ::fast_io::details::decay::scan_controls_scalar_fallback_select<
			transport, stack_policy>(in, t, args...);
	}
}

} // namespace decay
} // namespace details

namespace operations::decay
{

/// @brief Checks per-proxy preconditions before aggregate transport-budget arithmetic is considered.
/// @details A nested `if constexpr` is required here: ordinary `&&` syntax does not prevent every compiler from
///          instantiating a later variable template while substituting the complete expression. The ordered checks
///          reject references/cv-qualified expressions, incomplete types, nontrivial values, and oversized objects
///          before any operation that assumes the earlier property. Only exact unqualified non-lvalue expressions are
///          admitted; this also prevents by-value template deduction from silently dropping top-level cv and changing
///          which scanner CPO overload observes the proxy.
template <typename input, typename arg>
inline consteval bool scan_owned_proxy_individually_eligible_impl()
{
	if constexpr (!::std::same_as<arg, ::std::remove_cvref_t<arg>>)
	{
		return false;
	}
	else if constexpr (!requires { sizeof(arg); })
	{
		return false;
	}
	else if constexpr (sizeof(arg) >
				   ::fast_io::operations::decay::scan_owned_proxy_max_object_size)
	{
		// A type-level ABI override can prove a wider HFA/HVA or target extension direct, but this controller's pack
		// arithmetic and measured inline budget intentionally remain capped at the common two-word envelope.
		return false;
	}
	else if constexpr (!::fast_io::details::abi_small_trivial_argument_object<arg>())
	{
		// The copied proxies are function arguments, so only the argument policy is relevant here. In particular, MS
		// x64 accepts exact 1/2/4/8-byte aggregate classes while SysV AMD64 and AAPCS64 retain a two-word envelope.
		// Reusing the aggregate-result policy would incorrectly penalize asymmetric ABIs such as AAPCS32 and MIPS o32;
		// reusing this argument policy in a value-returning helper would make the opposite, sret-prone error.
		return false;
	}
	else if constexpr (alignof(arg) > alignof(::std::size_t))
	{
		// The word-rounded aggregate budget models ordinary descriptor slots. An over-aligned value may require
		// inter-argument padding or a platform-specific indirect ABI even when its language size is small.
		return false;
	}
	else if constexpr (!::std::is_trivially_move_constructible_v<arg>)
	{
		// The optional owner is initialized from the exact forwarded entry expression. The common repeated-copy proof
		// covers later named-lvalue calls, while this additional check proves that an rvalue cannot select a deleted or
		// user-defined move constructor before reaching that owner.
		return false;
	}
	else
	{
		return ::fast_io::value_transport_safe_scan_proxy<
			typename input::input_char_type, arg>;
	}
}

template <typename input, typename arg>
inline constexpr bool scan_owned_proxy_individually_eligible{
	::fast_io::operations::decay::scan_owned_proxy_individually_eligible_impl<input, arg>()};

/// @brief Admits only small, explicitly relocatable normalized non-lvalue proxy packs to the owner path.
/// @details The exact unqualified-type test is essential: a marked lvalue or cv-qualified xvalue may require identity
///          or overload preservation in the caller, and its exact category must survive. Trivial special members let
///          both the entry owner and the outlined by-value fallback copy without hidden work; the CPO supplies the
///          stronger semantic proof that
///          those copies preserve scan meaning. A pack failing any one condition retains the reference implementation,
///          unless the separate large-pack chunk policy below proves bounded value fallback safe. Layered constant
///          evaluation is deliberate: after every object has proved a two-word maximum and the whole-owner count is
///          bounded, the final `sizeof` fold has a mathematical upper bound and cannot overflow `size_t`.
template <typename input, typename... Args>
inline consteval bool scan_owned_proxy_pack_eligible_impl()
{
	if constexpr (sizeof...(Args) <= 1u ||
				  sizeof...(Args) > ::fast_io::operations::decay::scan_owned_proxy_max_count)
	{
		return false;
	}
	else if constexpr (
		!(::fast_io::operations::decay::scan_owned_proxy_individually_eligible<input, Args> && ...))
	{
		return false;
	}
	else
	{
		// Parameter ABIs allocate whole machine-word slots even for an object whose language `sizeof` is only one byte
		// above a word. Round each independently; summing raw sizes would admit twenty-eight 9-byte proxies to a nominal
		// 256-byte budget even though their word-slot footprint is 448 bytes. The per-object alignment gate above keeps
		// this a conservative bound for the ordinary descriptor class admitted by the policy.
		constexpr ::std::size_t word_size{sizeof(::std::size_t)};
		return (0u + ... + (((sizeof(Args) + word_size - 1u) / word_size) * word_size)) <=
			   ::fast_io::operations::decay::scan_owned_proxy_max_total_size;
	}
}

template <typename input, typename... Args>
inline constexpr bool scan_owned_proxy_pack_eligible{
	::fast_io::operations::decay::scan_owned_proxy_pack_eligible_impl<input, Args...>()};

/// @brief Admits a large marked pack to bounded value transport only in scalar fallback chunks.
/// @details The whole-entry count and total-byte caps are deliberately not applied to the complete pack here: no call
///          ever receives that complete pack by value. Each argument must independently satisfy the stronger semantic,
///          object-size, alignment, and triviality proof. The controller then copies at most the fixed worst-case chunk
///          count per no-inline call, which proves the original aggregate byte cap for every heterogeneous slice. A
///          single fold checks individual properties once; the execution helper advances in fixed slices and therefore
///          cannot recreate the old controller-per-suffix quadratic instantiation graph.
template <typename input, typename... Args>
inline consteval bool scan_chunked_proxy_pack_eligible_impl()
{
	if constexpr (sizeof...(Args) <=
				  ::fast_io::operations::decay::scan_owned_proxy_max_count)
	{
		return false;
	}
	else
	{
		return (::fast_io::operations::decay::scan_owned_proxy_individually_eligible<
			input, Args> && ...);
	}
}

template <typename input, typename... Args>
inline constexpr bool scan_chunked_proxy_pack_eligible{
	::fast_io::operations::decay::scan_chunked_proxy_pack_eligible_impl<input, Args...>()};

/// @brief Recognizes a precise-run controller shape for which measurements admit value transport.
/// @details A direct status CPO has no outlined scalar alternative. Most context-only packs enter that alternative
///          immediately and therefore retain exact references as well. A leading run of at least two precise scanners
///          is the general measured exception: keeping their marked proxy fields in SSA form avoids address escape on
///          the hot contiguous path. This predicate is shared by whole-entry ownership and chunked cold transport; its
///          source/shape proof remains orthogonal to semantic transport safety and call-shape capacity.
template <typename input, typename... Args>
inline consteval bool scan_owned_proxy_precise_prefix_available()
{
	using char_type = typename input::input_char_type;
	constexpr auto classification{
		::fast_io::details::decay::find_continuous_precise_scan_n<char_type, Args...>()};
	return classification.position > 1u;
}

/// @brief Recognizes the separately measured two-context-scanner by-value transport case.
/// @details The general context fallback remains outlined: inlining two refill state machines enlarged the timed loop
///          and introduced library-copy calls on GCC 15. Passing two marked pointer-sized descriptors by value instead
///          removes one proxy-address indirection while preserving that outline. On x86-64 Linux this reduced the
///          `context-pack` median by about 3.9%; an AArch64 Darwin control was neutral within run-to-run variation.
///
///          Exact cardinality and the one-word limit are part of the proof, not tuning folklore. Together with the
///          separately borrowed input observer the fallback receives three ordinary scalar slots, so AAPCS32/MIPS o32
///          do not split a two-word aggregate at their smaller register boundary, and stack ABIs do not carry more
///          payload than the reference form. `scan_owned_proxy_pack_eligible` is evaluated first and independently
///          proves exact unqualified values, trivial special members, native ABI admission, and the author's
///          `scan_proxy_value_transport_safe` promise. Single scanners already use direct scalar dispatch; packs larger
///          than two retain the linear reference controller until equivalent code-shape evidence exists.
template <typename input, typename... Args>
inline consteval bool scan_owned_proxy_context_pair_available()
{
	using char_type = typename input::input_char_type;
	if constexpr (sizeof...(Args) != 2u)
	{
		return false;
	}
	else if constexpr (!(::std::same_as<Args, ::std::remove_cvref_t<Args>> && ...))
	{
		// Keep this classifier safe in isolation as well as under the outer pack-eligibility gate. In particular, a
		// context-capable lvalue proxy must never be turned into a value merely because its language size is one word.
		return false;
	}
	else if constexpr (!(requires { sizeof(Args); } && ...))
	{
		return false;
	}
	else if constexpr (!(::fast_io::context_scannable<char_type, Args> && ...))
	{
		return false;
	}
	else
	{
		return ((sizeof(Args) <= sizeof(::std::size_t) &&
				 alignof(Args) <= alignof(::std::size_t)) && ...);
	}
}

/// @brief Dispatches one already-normalized scanner pack with a fixed proxy-transport policy.
/// @details Every controller operation observes named lvalues, regardless of whether the entry owns copies or retains
///          exact references. `transport` is therefore a type-level policy rather than a value-category probe at this
///          layer. Propagating it through suffix dispatch proves that one controller cannot switch transport policy
///          halfway through the pack. Mutex recursion occurs one level earlier while exact normalized expression
///          categories remain available. The input observer is a separate invariant: the public boundary establishes
///          one lifetime-valid observer, which may be caller-owned or materialized, and every helper receives it by
///          reference. The reference specialization performs no scanner-proxy copy, accepts noncopyable aliases, and
///          keeps the linear large-pack controller introduced to remove the former quadratic recursion/code-size
///          growth.
template <typename stack_policy, ::fast_io::details::decay::scan_proxy_fallback_transport transport,
		  typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay_impl(input &instm, Args &...args)
{
	if constexpr (::fast_io::operations::decay::defines::has_status_scan_define<input, Args &...>)
	{
		return status_scan_define(instm, args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<input>)
	{
		if constexpr (sizeof...(Args) == 1u)
		{
			// A one-target pack has no adjacent precise scanners to batch. Entering the recursive pack controller added
			// an otherwise unavoidable call in GCC for context-only scanners, while the structurally identical hybrid
			// specialization happened to inline. Direct scalar dispatch removes that concept-dependent codegen accident;
			// sanitizer staging and all scanner protocol selection remain centralized in `scan_single_impl`.
			return (::fast_io::details::scan_single_impl<stack_policy>(instm, args) && ...);
		}
		else
		{
			return ::fast_io::details::decay::scan_controls_impl<stack_policy, transport>(
				instm, args...);
		}
	}
	else if constexpr (::fast_io::operations::defines::available_add_ibuf<input>)
	{
		static_assert(::fast_io::operations::decay::defines::has_status_scan_define<input, Args &...>,
					  "If you want to scan this type of file, please add ::fast_io::basic_ibuf.");
		return false;
	}
	else
	{
		static_assert(::fast_io::operations::decay::defines::has_status_scan_define<input, Args &...>,
					  "type not scannable.");
		return false;
	}
}

/// @brief Establishes storage for a small pack whose proxy types prove value transport equivalent to reference use.
/// @details This helper is separate from the general dispatcher so proxy parameters, including the outlined fallback
///          call, are values only in the admitted specialization. The already-normalized input observer remains an
///          exact reference to the public entry's storage; owning proxy values must not introduce another observer copy
///          or reject a valid move-only stream-ref result. Inlining can scalar-replace the proxy values on the
///          contiguous fast path, while non-admitted packs never instantiate this by-value aggregate path.
template <typename stack_policy, typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay_owned(input &instm, Args... args)
{
	return ::fast_io::operations::decay::scan_freestanding_decay_impl<
		stack_policy,
		::fast_io::details::decay::scan_proxy_fallback_transport::whole_value>(instm, args...);
}

/// @brief Selects input strategy and bounded proxy transport from one stable normalized observer.
/// @details Public alias/forward temporaries live through this complete call expression. Mutex recursion forwards those
///          same normalized expressions without re-running alias or character forwarding, then materializes an
///          unlocked prvalue once or preserves an unlocked lvalue by reference. A direct status CPO retains references
///          because it has no controller address-escape problem. A small eligible controller shape establishes compact
///          owned values: either a selected precise prefix or the independently bounded two-context-scanner transport
///          case.
///          A larger eligible pack keeps those exact boundary references and permits copies only inside bounded cold
///          chunks. Every scanner CPO below still observes a named lvalue. This preserves the entry-decay invariant:
///          normalization narrows the type range once, and deeper layers consume stable specializations instead of
///          rebuilding a forwarding-reference graph.
template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay_dispatch(input &instm, Args &&...args)
{
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<input>)
	{
		if constexpr (
			::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<input>)
		{
			// Locking is a stream-level semantic boundary and therefore precedes status and ibuffer strategies. The named
			// local below is an owner only for a prvalue result; decltype(auto) preserves an lvalue result as the device's
			// existing cursor object. Thus recursion never reopens a blanket by-value transport boundary.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
			decltype(auto) unlocked =
				::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm);
			return ::fast_io::operations::decay::scan_freestanding_decay_dispatch<stack_policy>(
				unlocked, ::std::forward<Args>(args)...);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<input>,
				"fast_io: an input mutex CPO requires a storable lock/unlock proxy and a type-progressing, "
				"character-preserving unlocked input reference");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_status_scan_define<input, Args &...>)
	{
		return ::fast_io::operations::decay::scan_freestanding_decay_impl<
			stack_policy,
			::fast_io::details::decay::scan_proxy_fallback_transport::reference>(instm, args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<input>)
	{
		if constexpr (sizeof...(Args) == 1u)
		{
			// A scalar ibuffer record has no aggregate transport or batching decision. Entering the variadic controller
			// here made Clang outline `scan_freestanding_decay_impl` even though that function immediately selected the
			// same leaf. Keep this one-target shape isolated at the dispatch boundary; larger packs and status/mutex
			// protocols retain their existing owners.
			return (::fast_io::details::scan_single_impl<stack_policy>(instm, args) && ...);
		}
		if constexpr (
			::fast_io::operations::decay::scan_owned_proxy_pack_eligible<input, Args...>)
		{
			if constexpr (
				::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<input, Args...>() ||
				::fast_io::operations::decay::scan_owned_proxy_context_pair_available<input, Args...>())
			{
				return ::fast_io::operations::decay::scan_freestanding_decay_owned<stack_policy>(
					instm, ::std::forward<Args>(args)...);
			}
		}
		if constexpr (
			::fast_io::operations::decay::scan_chunked_proxy_pack_eligible<input, Args...>)
		{
			if constexpr (
				::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<input, Args...>())
			{
				return ::fast_io::operations::decay::scan_freestanding_decay_impl<
					stack_policy,
					::fast_io::details::decay::scan_proxy_fallback_transport::chunked_value>(
					instm, args...);
			}
		}
		return ::fast_io::operations::decay::scan_freestanding_decay_impl<
			stack_policy,
			::fast_io::details::decay::scan_proxy_fallback_transport::reference>(instm, args...);
	}
	else
	{
		return ::fast_io::operations::decay::scan_freestanding_decay_impl<
			stack_policy,
			::fast_io::details::decay::scan_proxy_fallback_transport::reference>(instm, args...);
	}
}

/// @brief Owns a normalized input observer supplied through the explicit by-value compatibility entry.
/// @details The function parameter is intentionally a value: an lvalue follows ordinary parameter-copy semantics, and
///          a prvalue observer obtains storage exactly once. All strategy dispatch below this wrapper borrows that
///          storage, so controller recursion cannot multiply observer construction.
template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay(input instm, Args &&...args)
{
	return ::fast_io::operations::decay::scan_freestanding_decay_dispatch<stack_policy>(
		instm, ::std::forward<Args>(args)...);
}

/// @brief Borrows a stable observer returned by the public stream-reference normalization CPO.
/// @details Non-trivial stream-reference lvalues denote mutable cursor identity. Copying one at this boundary can
///          duplicate state, add an outlined copy call, or reject a move-only observer, without extending any source
///          lifetime. This overload is deliberately named rather than overloaded on `T&`: a low-level caller choosing
///          the owning by-value compatibility boundary remains unambiguous.
template <typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay_borrowed_input(
	input &instm, Args &&...args)
{
	return ::fast_io::operations::decay::scan_freestanding_decay_dispatch<stack_policy>(
		instm, ::std::forward<Args>(args)...);
}

} // namespace operations::decay

} // namespace fast_io

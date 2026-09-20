#pragma once

/*
 * In-memory print/scan conversion pipeline (IO level).
 *
 * `to` and `inplace_to` compose existing printable producers with scannable
 * consumers through bounded intermediate character fragments. This file owns
 * the operation-level choice between repeatable scatter, reserve storage,
 * dynamic materialization, and context scanning, including progress and EOF
 * validation. It introduces no format grammar and no new leaf representation:
 * both sides reuse the ordinary print/scan CPO vocabulary without a device.
 */

namespace fast_io
{

namespace details
{

/// @brief Forms the terminal pointer of a scatter without performing arithmetic on an empty null view.
/// @details `basic_io_scatter_t` permits the conventional empty representation `{nullptr, 0}`. Although no character
///          is dereferenced, spelling `base + len` for that representation is not valid C++ pointer arithmetic because
///          null does not designate an array. Returning `base` for zero length preserves the exact empty range; a
///          positive length retains the producer's ordinary requirement that `base` starts a live character array.
template <::std::integral char_type>
inline constexpr char_type const *scan_scatter_end(
	char_type const *base, ::std::size_t length) noexcept
{
	return length == 0u ? base : base + length;
}

/// @brief Proves the scatter expression actually issued by the by-value `to` dispatcher.
/// @details `basic_inplace_to_decay` owns each normalized argument by value and every helper subsequently names that
///          object. Its CPO expression is therefore `T&`, not the public compatibility query's `T&&`. Keeping this fact
///          in one predicate prevents an rvalue-only scatter overload from selecting a strategy whose body calls it as
///          an lvalue.
template <::std::integral char_type, typename T>
inline constexpr bool to_named_scatter_printable_v =
	::fast_io::scatter_printable_for<char_type, T &>;

/// @brief Selects a scatter for length-then-copy conversion only when its observation is repeatable.
/// @details A single-fragment context conversion may consume a scatter immediately and needs no retained-lifetime proof.
///          A contiguous target with several fragments first sums lengths and later asks every producer for bytes again.
///          The borrowed marker is the source-side promise that both calls return the same live character sequence. If
///          the marker is absent but a reserve protocol exists, the two-pass strategy uses that reserve protocol instead;
///          a scatter-only producer falls back to one-pass dynamic materialization.
template <::std::integral char_type, typename T>
inline constexpr bool to_repeatable_named_scatter_v =
	::fast_io::details::to_named_scatter_printable_v<char_type, T> &&
	::fast_io::borrowed_scatter_source<char_type, T>;

template <::std::integral char_type, typename T>
inline constexpr bool to_two_pass_fragment_available_v =
	::fast_io::details::to_repeatable_named_scatter_v<char_type, T> ||
	::fast_io::reserve_printable<char_type, T> ||
	::fast_io::dynamic_reserve_printable<char_type, T>;

/// @brief Recognizes normalized source shapes whose token spelling can be checked before direct string emission.
/// @details `inplace_to`'s default string scanner stops at C-space.  A direct append is therefore valid only for a
///          source whose actual spelling is known to contain no C-space.  Integer leaves carry a type-level proof;
///          static literal leaves are checked from their retained scatter at the operation boundary.  Unknown/custom
///          producers deliberately remain on the context scanner, even when they happen to expose a reserve CPO.
template <typename char_type, typename T>
struct to_c_space_free_chvw_shape : ::std::false_type
{
};

template <typename char_type, typename value_type>
struct to_c_space_free_chvw_shape<
	char_type, ::fast_io::manipulators::chvw_t<value_type>>
	: ::std::bool_constant<::std::integral<::std::remove_cvref_t<value_type>>>
{
};

template <typename char_type, typename T>
struct to_c_space_free_scatter_shape : ::std::false_type
{
};

template <typename char_type>
struct to_c_space_free_scatter_shape<
	char_type, ::fast_io::basic_io_scatter_t<char_type>> : ::std::true_type
{
};

template <typename char_type>
struct to_c_space_free_scatter_shape<
	char_type, ::fast_io::basic_prfch_cacheable_io_scatter_t<char_type>> : ::std::true_type
{
};

template <typename T>
struct to_c_space_free_builtin_scalar_shape : ::std::false_type
{
};

template <::fast_io::manipulators::scalar_flags flags, typename T>
struct to_c_space_free_builtin_scalar_shape<
	::fast_io::manipulators::scalar_manip_t<flags, T>>
	: ::std::bool_constant<
		  ::fast_io::details::my_integral<::std::remove_cvref_t<T>> ||
		  ::fast_io::details::my_floating_point<::std::remove_cvref_t<T>>>
{
};

template <typename char_type, typename T>
inline constexpr bool to_c_space_free_source_shape_v =
	::std::integral<char_type> &&
	((to_c_space_free_builtin_scalar_shape<::std::remove_cvref_t<T>>::value &&
	  ::fast_io::c_space_free_print_fragment<char_type, T>) ||
	::fast_io::details::decay::print_static_scatter_traits<
		char_type, ::std::remove_cvref_t<T>>::available ||
	to_c_space_free_chvw_shape<char_type, ::std::remove_cvref_t<T>>::value ||
	to_c_space_free_scatter_shape<char_type, ::std::remove_cvref_t<T>>::value);

template <typename char_type, typename T>
inline constexpr bool to_c_space_free_source_value(T &value) noexcept
{
	static_assert(::std::integral<char_type>);
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (
		to_c_space_free_builtin_scalar_shape<value_type>::value &&
		::fast_io::c_space_free_print_fragment<char_type, value_type>)
	{
		return true;
	}
	else if constexpr (::fast_io::details::decay::print_static_scatter_traits<char_type, value_type>::available)
	{
		auto const scatter{
			::fast_io::details::decay::print_static_scatter_traits<char_type, value_type>::define(value)};
		// The canonical empty scatter is {nullptr, 0}; do not form null pointer arithmetic while checking it.
		for (::std::size_t index{}; index != scatter.len; ++index)
		{
			if (::fast_io::char_category::is_c_space(scatter.base[index]))
				return false;
		}
		return true;
	}
	else if constexpr (to_c_space_free_chvw_shape<char_type, value_type>::value)
	{
		using output_unsigned_type = ::std::make_unsigned_t<::std::remove_cv_t<char_type>>;
		char_type const emitted{static_cast<char_type>(
			static_cast<output_unsigned_type>(value.reference))};
		return !::fast_io::char_category::is_c_space(emitted);
	}
	else if constexpr (to_c_space_free_scatter_shape<char_type, value_type>::value)
	{
		for (::std::size_t index{}; index != value.len; ++index)
		{
			if (::fast_io::char_category::is_c_space(value.base[index]))
				return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

template <typename char_type, typename... Args>
inline constexpr bool to_c_space_free_source_values(Args &...args) noexcept
{
	return (to_c_space_free_source_value<char_type>(args) && ...);
}

template <typename char_type, typename T>
inline constexpr bool to_source_has_emitted_character(T &value) noexcept
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (to_c_space_free_chvw_shape<char_type, value_type>::value)
	{
		return true;
	}
	else if constexpr (to_c_space_free_scatter_shape<char_type, value_type>::value)
	{
		return value.len != 0u;
	}
	else if constexpr (::fast_io::details::decay::print_static_scatter_traits<char_type, value_type>::available)
	{
		return ::fast_io::details::decay::print_static_scatter_traits<char_type, value_type>::define(value).len != 0u;
	}
	else
	{
		// Library-owned scalar carriers admitted by the source-shape marker always have a non-empty lexical spelling.
		return true;
	}
}

template <typename char_type, typename... Args>
inline constexpr bool to_source_values_have_emitted_character(Args &...args) noexcept
{
	return (to_source_has_emitted_character<char_type>(args) || ...);
}

/// @brief A destination-specific direct append operation for proven whitespace-free `inplace_to` records.
/// @details The operation itself is an ADL customization so only a destination that audited clear/append semantics can
///          opt in.  It is intentionally separate from the ordinary output-buffer marker: a put area alone says
///          nothing about the scanner's token grammar or whether the target must be reset before emission.
template <typename char_type, typename T, typename... Args>
concept inplace_to_direct_printable =
	::std::integral<char_type> && (sizeof...(Args) != 0u) &&
	(to_c_space_free_source_shape_v<char_type, Args> && ...) &&
	requires(T &target, Args &...args) {
		inplace_to_direct_print_define(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
			target, args...);
	};

template <typename char_type, typename T, typename... Args>
concept inplace_to_direct_source_safety_query =
	::std::integral<char_type> &&
	requires(T &target, Args &...args) {
		{
			inplace_to_direct_print_source_safe(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
				target, args...)
		} -> ::std::same_as<bool>;
	};

template <typename char_type, typename T, typename... Args>
inline constexpr bool inplace_to_direct_source_values_safe(T &target, Args &...args) noexcept
{
	if constexpr (inplace_to_direct_source_safety_query<char_type, T, Args...>)
	{
		return inplace_to_direct_print_source_safe(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, target, args...);
	}
	else
	{
		return true;
	}
}

/// @brief Feeds one materialized print fragment to a context scanner.
/// @return `true` exactly when the scanner reports completion; `false` when the fragment is exhausted while partial.
/// @details A parse code and an iterator answer independent questions. `ok` may be returned at the fragment end, while
///          `partial` may consume only a prefix because the state machine changed phase. Therefore completion is proved
///          only by the code, and the iterator is used only to select the next suffix. A partial result that consumes no
///          available input violates the progress contract and is rejected instead of entering an infinite loop.
template <::std::integral char_type, typename state, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool inplace_to_decay_context_consume(state &s, T &t, char_type const *first,
												char_type const *last)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	auto current{first};
	while (current != last)
	{
		auto [it, ec] = scan_context_define(
			io_reserve_type<char_type, scanner_type>, s, current, last, t);
		// The CPO result must designate this supplied fragment. Check membership before accepting even `ok`, because a
		// successful code paired with an escaped iterator would otherwise make the next suffix and pointer arithmetic
		// invalid; this is a range proof, not a statement about ownership of the underlying character storage.
		if constexpr (!::fast_io::context_scanner_result_in_range<char_type, T>)
		{
			if (!::fast_io::details::scan_iterator_in_current_chunk(current, last, it)) [[unlikely]]
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
			}
		}
		if (ec == ::fast_io::parse_code::ok)
		{
			return true;
		}
		if (ec != ::fast_io::parse_code::partial)
		{
			::fast_io::throw_parse_code(ec);
		}
		if (it == current) [[unlikely]]
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
		}
		current = it;
	}
	return false;
}

/// @brief Applies the single terminal transition shared by every fragmented `inplace_to` strategy.
/// @details Fragment production and EOF finalization are deliberately split: each producer may choose scatter,
///          caller-owned reserve storage, or a dynamic buffer, but all of them have exactly one terminal boundary.
///          Centralizing that boundary proves the EOF CPO is invoked once and normalizes `partial` to `invalid`, since
///          no future fragment exists that could make an unfinished state productive.
template <::std::integral char_type, typename state, typename T>
inline constexpr void inplace_to_decay_context_finish(state &s, T &t)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	auto const code{scan_context_eof_define(io_reserve_type<char_type, scanner_type>, s, t)};
	if (code == ::fast_io::parse_code::ok)
	{
		return;
	}
	::fast_io::throw_parse_code(
		code == ::fast_io::parse_code::partial ? ::fast_io::parse_code::invalid : code);
}

/// @brief Borrows the normalized source pack throughout every recursive `to` strategy.
/// @details The inplace wrapper or value-returning `basic_to_decay` is the sole ownership boundary for a given call;
///          its by-value pack keeps prvalue aliases alive for the complete conversion. A recursive suffix walker
///          therefore has neither ownership nor forwarding work
///          left to perform.  Passing the named proxies by value copied a suffix again at every recursion depth,
///          producing theta(N^2) constructions and rejecting a valid move-only alias after admission.  References
///          preserve the original left-to-right CPO observations, make the execution expression match the concepts'
///          named-lvalue model, and reduce the recursive work to theta(N) without extending any lifetime.

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
/// @brief Materializes and scans exactly one printable argument before deciding whether another argument is needed.
/// @details This is the dynamic fallback paired with `scan_context_define`, not the whole-pack high-throughput print
///          path. A context scanner carries its parse state across fragment boundaries and can report `ok` as soon as
///          the minimum printed prefix determines the result--for example, once an integer's terminating delimiter is
///          observed. Printing the complete argument pack first would format producers which the scanner never needs,
///          execute their observable side effects, and change peak storage from the largest required fragment to the
///          sum of every fragment. The reserve/scatter branches above already coalesce packs whose complete size and
///          replay behavior are proved. This fallback instead prioritizes obtaining the exact scan result from the
///          least output required by the context protocol; it reuses one dynamic buffer and stops immediately on `ok`.
inline constexpr void
inplace_to_decay_context_impl(basic_dynamic_output_buffer_ref<basic_dynamic_output_buffer<char_type>> buffer, state &s,
							  T &t, Arg1 &arg, Args &...args)
{
	// Keep this call single-argument. Combining `arg, args...` would erase the context scanner's early-completion and
	// bounded-fragment properties even though it can look faster as an isolated print operation.
	::fast_io::operations::decay::print_freestanding_decay_impl<false>(buffer, arg);

	char_type *buffer_beg{buffer.dob_ptr->begin_ptr};
	char_type const *buffer_begin{buffer_beg};
	char_type const *buffer_curr{buffer.dob_ptr->curr_ptr};
	if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
	{
		return;
	}
	if constexpr (sizeof...(Args) != 0)
	{
		// The scanner consumed this fragment while retaining `s`; rewind only the output cursor so the next argument can
		// reuse the same allocation. Character storage is not required after `scan_context_define` returns partial.
		buffer.dob_ptr->curr_ptr = buffer_beg;
		inplace_to_decay_context_impl(buffer, s, t, args...);
	}
	else
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void inplace_to_decay_buffer_scatter_context_impl(state &s, T &t, Arg1 &arg, Args &...args)
{
	basic_io_scatter_t<char_type> scatter{print_scatter_define(io_reserve_type<char_type, Arg1>, arg)};
	char_type const *buffer_begin{scatter.base};
	char_type const *buffer_curr{
		::fast_io::details::scan_scatter_end(buffer_begin, scatter.len)};
	if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
	{
		return;
	}
	if constexpr (sizeof...(Args) != 0)
	{
		inplace_to_decay_buffer_scatter_context_impl<char_type>(s, t, args...);
	}
	else
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void inplace_to_decay_buffer_context_impl(char_type *buffer, state &s, T &t, Arg1 &arg, Args &...args)
{
	if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, Arg1> &&
				  ((::fast_io::details::to_named_scatter_printable_v<char_type, Args> && ...)))
	{
		inplace_to_decay_buffer_scatter_context_impl<char_type>(s, t, arg, args...);
	}
	else
	{
		char_type const *buffer_begin;
		char_type const *buffer_curr;
		if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, Arg1>)
		{
			auto scatter{print_scatter_define(io_reserve_type<char_type, Arg1>, arg)};
			buffer_begin = scatter.base;
			buffer_curr = ::fast_io::details::scan_scatter_end(buffer_begin, scatter.len);
		}
		else
		{

			buffer_curr = print_reserve_define(io_reserve_type<char_type, Arg1>, buffer, arg);
			buffer_begin = buffer;
		}
		if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
		{
			return;
		}
		if constexpr (sizeof...(Args) != 0)
		{
			inplace_to_decay_buffer_context_impl(buffer, s, t, args...);
		}
		else
		{
			::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
		}
	}
}

/// @brief Returns the byte-capped local staging capacity shared by `to`'s small stack plans.
/// @details The operation cap is deliberately independent of the potentially much larger configured GNU/Linux stack
///          budget. A stricter user-configured budget remains authoritative, and wide character domains round down to
///          a whole number of code units.
template <::std::integral char_type>
inline consteval ::std::size_t to_small_stack_capacity() noexcept
{
	constexpr ::std::size_t preferred_bytes{256u};
	constexpr ::std::size_t preferred_size{preferred_bytes / sizeof(char_type)};
	constexpr ::std::size_t configured_size{
		::fast_io::details::decay::print_stack_buffer_max_size<char_type>()};
	return configured_size < preferred_size ? configured_size : preferred_size;
}

/// @brief Proves that one bounded-context fragment can consume the operation-local stack scratch.
/// @details A retained scatter never writes into scratch. A static reserve writer counts only when its exact type-level
///          bound fits the nonzero operation cap; otherwise a mixed pack must not acquire an array which that writer and
///          every unhinted dynamic suffix are unable to use. A dynamic writer needs either the non-fatal bounded protocol
///          or a nonzero producer-authored stack preference. This predicate controls storage existence only; the
///          per-object size test below remains the capacity proof.
template <::std::integral char_type, typename T>
inline constexpr bool to_bounded_fragment_stack_scratch_candidate_v = []() consteval {
	constexpr ::std::size_t operation_capacity{
		::fast_io::details::to_small_stack_capacity<char_type>()};
	if constexpr (operation_capacity == 0u)
	{
		return false;
	}
	else if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::reserve_printable<char_type, T>)
	{
		return print_reserve_size(::fast_io::io_reserve_type<
			char_type, ::std::remove_cvref_t<T>>) <= operation_capacity;
	}
	else if constexpr (::fast_io::single_pass_bounded_materialization_source<char_type, T>)
	{
		return true;
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, T>)
	{
		return print_reserve_static_stack_size(
				   ::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u;
	}
	else
	{
		return false;
	}
}();

/// @brief Replaces insufficient reusable staging while preserving single ownership.
/// @pre `required_capacity` is in `[1,SIZE_MAX/sizeof(char_type)]`; the current owner is canonical--null, or non-null
///      with capacity in that same interval--and is empty or too small for the request.
/// @details The first allocation is at least `initial_capacity`; every later growth is
///          `max(required_capacity,2*C)`, with saturation at the largest character count representable by the allocator.
///          The replacement is acquired before ownership fields are exchanged, so its destructor releases the old block
///          only after the new block is owned. No bytes are copied: the context scanner has synchronously consumed the
///          preceding fragment before growth is permitted. This cold leaf is kept out of line because the pack walker
///          has one hot capacity check per reached source; cloning allocation, saturation, and ownership exchange at
///          every depth increases text without accelerating reuse.
template <::std::size_t initial_capacity, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr char_type *inplace_to_reusable_heap_buffer_grow(
	::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	::std::size_t required_capacity) noexcept
{
	constexpr ::std::size_t maximum_capacity{SIZE_MAX / sizeof(char_type)};
	static_assert(initial_capacity <= maximum_capacity);
	::std::size_t replacement_capacity;
	if (heap_buffer.ptr == nullptr)
	{
		constexpr ::std::size_t initial_nonzero_capacity{initial_capacity == 0u ? 1u : initial_capacity};
		replacement_capacity = required_capacity < initial_nonzero_capacity
			? initial_nonzero_capacity
			: required_capacity;
	}
	else
	{
		// Canonical ownership gives `C <= M`, so `M-C` is defined. If `C > M-C`, mathematical `2*C` exceeds
		// `M` and must saturate; otherwise `C+C <= M`, proving that the addition cannot wrap.
		::std::size_t const doubled_capacity{heap_buffer.size > maximum_capacity - heap_buffer.size
			? maximum_capacity
			: heap_buffer.size + heap_buffer.size};
		replacement_capacity = required_capacity < doubled_capacity ? doubled_capacity : required_capacity;
	}

	::fast_io::details::local_operator_new_array_ptr<char_type> replacement(replacement_capacity);
	char_type *const previous_ptr{heap_buffer.ptr};
	::std::size_t const previous_size{heap_buffer.size};
	heap_buffer.ptr = replacement.ptr;
	heap_buffer.size = replacement.size;
	replacement.ptr = previous_ptr;
	replacement.size = previous_size;
	return heap_buffer.ptr;
}

/// @brief Returns reusable heap staging whose capacity covers the current fragment bound.
/// @pre The owner is canonical: `(ptr == nullptr && size == 0) || (ptr != nullptr && size >= 1)`.
/// @details Let `C` be `heap_buffer.size` and `R` the requested character count. Normalization gives
///          `required_capacity = max(R,1) >= 1`; therefore `required_capacity <= C` implies `C >= 1`, and canonical
///          ownership formally excludes a null pointer without a second run-time test. On return, the owner is non-null
///          and has `C >= max(R,1)`. A miss enters the isolated growth transaction above. This function observes no
///          source object and therefore cannot make a suffix eager.
template <::std::size_t initial_capacity, ::std::integral char_type>
inline constexpr char_type *inplace_to_reusable_heap_buffer_ensure(
	::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	::std::size_t requested_capacity) noexcept
{
	constexpr ::std::size_t maximum_capacity{SIZE_MAX / sizeof(char_type)};
	if (requested_capacity > maximum_capacity) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	::std::size_t const required_capacity{requested_capacity == 0u ? 1u : requested_capacity};
	if (required_capacity <= heap_buffer.size) [[likely]]
	{
		return heap_buffer.ptr;
	}
	return ::fast_io::details::inplace_to_reusable_heap_buffer_grow<initial_capacity>(
		heap_buffer, required_capacity);
}

/// @brief Scans one current reserve/scatter fragment using bounded stack scratch when its own policy permits it.
/// @details Run-time reserve sizing is intentionally performed only for `arg`. A scanner which completes here prevents
///          every suffix size query and formatter call, preserving the historical lazy suffix semantics. Dynamic
///          producers with the non-fatal bounded-size protocol may use this operation's 256-byte cap directly; other
///          dynamic producers require the existing static-stack hint and must fit both limits. Larger current fragments
///          use one conversion-owned dynamic allocation, growing geometrically only when a later reached fragment has a
///          larger bound. Reuse changes storage cost only: every size query, writer, scanner transition, and early stop
///          remains in its original left-to-right position.
template <::std::size_t scratch_capacity, ::std::size_t heap_initial_capacity, ::std::integral char_type,
		  typename state, typename T, typename Arg>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool inplace_to_decay_bounded_buffer_context_one(
	char_type *scratch, ::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	state &s, T &t, Arg &arg)
{
	bool completed{};
	if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, Arg>)
	{
		auto const scatter{print_scatter_define(io_reserve_type<char_type, Arg>, arg)};
		auto const first{scatter.base};
		auto const last{::fast_io::details::scan_scatter_end(first, scatter.len)};
		completed = ::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, first, last);
	}
	else
	{
		::std::size_t reserve_size;
		bool use_stack{};
		if constexpr (::fast_io::dynamic_reserve_printable<char_type, Arg>)
		{
			if constexpr (scratch_capacity != 0u &&
						  ::fast_io::single_pass_bounded_materialization_source<char_type, Arg>)
			{
				::std::size_t maximum_size{scratch_capacity};
				if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, Arg>)
				{
					constexpr ::std::size_t producer_capacity{
						print_reserve_static_stack_size(io_reserve_type<char_type, Arg>)};
					if (producer_capacity < maximum_size)
					{
						maximum_size = producer_capacity;
					}
				}
				if (maximum_size != 0u)
				{
					reserve_size = ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
						arg, maximum_size);
					use_stack = reserve_size != SIZE_MAX && reserve_size <= maximum_size;
				}
			}
			if (!use_stack)
			{
				reserve_size = print_reserve_size(io_reserve_type<char_type, Arg>, arg);
				if constexpr (scratch_capacity != 0u &&
							  ::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, Arg>)
				{
					constexpr ::std::size_t producer_capacity{
						print_reserve_static_stack_size(io_reserve_type<char_type, Arg>)};
					use_stack = reserve_size <= scratch_capacity && reserve_size <= producer_capacity;
				}
			}
		}
		else
		{
			reserve_size = print_reserve_size(io_reserve_type<char_type, Arg>);
			if constexpr (scratch_capacity != 0u)
			{
				use_stack = reserve_size <= scratch_capacity;
			}
		}

		if (use_stack)
		{
			auto const last{print_reserve_define(io_reserve_type<char_type, Arg>, scratch, arg)};
			auto const scratch_end{reserve_size == 0u ? scratch : scratch + reserve_size};
			if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
					scratch, scratch_end, last)) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			completed = ::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, scratch, last);
		}
		else
		{
			auto const first{
				::fast_io::details::inplace_to_reusable_heap_buffer_ensure<heap_initial_capacity>(
					heap_buffer, reserve_size)};
			auto const last{print_reserve_define(io_reserve_type<char_type, Arg>, first, arg)};
			auto const buffer_end{reserve_size == 0u ? first : first + reserve_size};
			if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
					first, buffer_end, last)) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			completed = ::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, first, last);
		}
	}
	return completed;
}

/// @brief Walks bounded fragments left-to-right and stops before observing the first unneeded suffix.
/// @details Built-in `||` is sequenced and short-circuiting. Consequently the fold invokes the one-fragment operation
///          for source `i+1` exactly when every reached source through `i` returned partial. A successful transition
///          suppresses all later size and writer CPOs; if every transition is partial, the single post-fold edge invokes
///          EOF exactly once. Unlike suffix recursion, this control form also shares its terminal cleanup edge without
///          changing the owned source pack or any fragment representation.
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
	!defined(__CUDACC__) && defined(__OPTIMIZE__) && !defined(__OPTIMIZE_SIZE__) && \
	(__GNUC__ == 12 || __GNUC__ == 13 || __GNUC__ >= 15) && defined(__AVX__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))

/// @brief Selects the GCC large-pack code-generation exception without changing the protocol graph.
/// @details Let `N` be the number of already-decayed source objects. Native x86-64 measurements found that GCC 12 and
///          GCC 15 onward clone and alias-version each inlined reserve writer at `N >= 8`; GCC 13 crosses the same
///          profitability boundary at `N >= 16`. GCC 11 regressed and GCC 14 was neutral-to-slower, so they are excluded
///          rather than being hidden behind an imprecise version range. The highest tested policy is inherited by later
///          GCC releases, while AVX-disabled, size-optimized, non-x86, and non-GCC builds retain the ordinary optimizer.
template <::std::size_t source_count>
inline constexpr bool gcc_inplace_to_large_pack_no_loop_vectorize{
	source_count >= (__GNUC__ == 13 ? 16u : 8u)};

/// @brief Implements the common large/small pack state machine inside the selected GCC optimization domain.
/// @pre `scratch`, `heap_buffer`, `s`, `t`, and every source reference satisfy the preconditions of
///      `inplace_to_decay_bounded_buffer_context_one`; their lifetimes cover this call.
/// @post For the least reached index `j` whose scanner completes, sources `[0,j]` are observed exactly once and sources
///       `(j,N)` are not observed. If no such `j` exists, every source is observed once and EOF is invoked exactly once.
/// @details The forced inline is local to the two wrappers below. Consequently their only difference is GCC's loop
///          vectorization policy: no CPO is duplicated, reordered, or made eager. All arguments remain references to the
///          decay-owned objects created by the public front door; this helper introduces neither another decay nor an
///          ABI-visible reference layer. Keeping the fold in one body also makes the formal transition graph identical
///          for the ordinary and exceptional instantiations.
template <::std::size_t scratch_capacity, ::std::size_t heap_initial_capacity, ::std::integral char_type,
		  typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr void inplace_to_decay_bounded_buffer_context_walk(
	char_type *scratch, ::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	state &s, T &t, Arg1 &arg, Args &...args)
{
	bool const completed{
		::fast_io::details::inplace_to_decay_bounded_buffer_context_one<
			scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, arg) ||
		(... || ::fast_io::details::inplace_to_decay_bounded_buffer_context_one<
			scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, args))};
	if (!completed)
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

template <::std::size_t scratch_capacity, ::std::size_t heap_initial_capacity, ::std::integral char_type,
		  typename state, typename T, typename Arg1, typename... Args>
	requires(!gcc_inplace_to_large_pack_no_loop_vectorize<sizeof...(Args) + 1u>)
inline constexpr void inplace_to_decay_bounded_buffer_context_impl(
	char_type *scratch, ::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	state &s, T &t, Arg1 &arg, Args &...args)
{
	::fast_io::details::inplace_to_decay_bounded_buffer_context_walk<
		scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, arg, args...);
}

/// @brief Applies the measured GCC policy only after the large-pack threshold is proven at instantiation time.
/// @details Disabling loop vectorization prevents GCC from cloning a main vector loop, scalar epilogues, and run-time
///          alias checks at every reserve-producing pack position. SLP and explicit SIMD remain enabled. The attribute
///          can inhibit caller inlining, which is why the constrained overload is never viable below the measured
///          threshold; large packs amortize that single boundary and reduce both dynamic branches and generated text.
template <::std::size_t scratch_capacity, ::std::size_t heap_initial_capacity, ::std::integral char_type,
		  typename state, typename T, typename Arg1, typename... Args>
	requires(gcc_inplace_to_large_pack_no_loop_vectorize<sizeof...(Args) + 1u>)
[[gnu::optimize("no-tree-loop-vectorize")]]
inline constexpr void inplace_to_decay_bounded_buffer_context_impl(
	char_type *scratch, ::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	state &s, T &t, Arg1 &arg, Args &...args)
{
	::fast_io::details::inplace_to_decay_bounded_buffer_context_walk<
		scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, arg, args...);
}

#else

template <::std::size_t scratch_capacity, ::std::size_t heap_initial_capacity, ::std::integral char_type,
		  typename state, typename T, typename Arg1, typename... Args>
inline constexpr void inplace_to_decay_bounded_buffer_context_impl(
	char_type *scratch, ::fast_io::details::local_operator_new_array_ptr<char_type> &heap_buffer,
	state &s, T &t, Arg1 &arg, Args &...args)
{
	bool const completed{
		::fast_io::details::inplace_to_decay_bounded_buffer_context_one<
			scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, arg) ||
		(... || ::fast_io::details::inplace_to_decay_bounded_buffer_context_one<
			scratch_capacity, heap_initial_capacity, char_type>(scratch, heap_buffer, s, t, args))};
	if (!completed)
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

#endif

template <::std::integral char_type, bool ln, typename T, typename... Args>
inline constexpr ::std::size_t calculate_print_normal_maxium_size_main(::std::size_t mx_value) noexcept
{
	::std::size_t val{};
	if constexpr (ln && (sizeof...(Args) == 0))
	{
		++val;
	}
	if constexpr (reserve_printable<char_type, T>)
	{
		constexpr ::std::size_t size{print_reserve_size(io_reserve_type<char_type, T>)};
		static_assert(size != SIZE_MAX, "overflow");
		val += size;
	}
	if (mx_value < val)
	{
		mx_value = val;
	}
	if constexpr ((sizeof...(Args) == 0))
	{
		return mx_value;
	}
	else
	{
		return calculate_print_normal_maxium_size_main<char_type, ln, Args...>(mx_value);
	}
}

template <::std::integral char_type, bool ln, typename... Args>
inline constexpr ::std::size_t calculate_print_normal_maxium_size() noexcept
{
	return calculate_print_normal_maxium_size_main<char_type, ln, Args...>(0);
}

template <::std::integral char_type, bool ln, typename T, typename... Args>
inline constexpr ::std::size_t
calculate_print_normal_dynamic_maxium_main(::std::size_t mx_value, T &t, Args &...args)
{
	::std::size_t size{};
	if constexpr (dynamic_reserve_printable<char_type, T>)
	{
		size = print_reserve_size(io_reserve_type<char_type, T>, t);
	}
	else if constexpr (reserve_printable<char_type, T>)
	{
		// The dynamic path reuses one fragment buffer for both run-time and type-level reserve producers. Ignoring a
		// static-only producer here made the maximum smaller than a later write whenever another argument forced this path.
		size = print_reserve_size(io_reserve_type<char_type, T>);
	}
	if constexpr (dynamic_reserve_printable<char_type, T> || reserve_printable<char_type, T>)
	{
		if constexpr (ln && (sizeof...(Args) == 0))
		{
			if (size == SIZE_MAX)
			{
				fast_terminate();
			}
			++size;
		}
		if (mx_value < size)
		{
			mx_value = size;
		}
	}
	if constexpr ((sizeof...(Args) == 0))
	{
		return mx_value;
	}
	else
	{
		return calculate_print_normal_dynamic_maxium_main<char_type, ln>(mx_value, args...);
	}
}

template <::std::integral char_type, typename T>
inline constexpr void deal_with_single_to(char_type const *buffer_begin, char_type const *buffer_end, T &t)
{
	// The normalized scanner is deliberately borrowed. An alias CPO may return a noncopyable lvalue proxy, and the
	// enclosing public call already guarantees that either its referenced storage or its prvalue temporary remains alive.
	// Its cv-qualification is part of CPO overload resolution, while the reserve tag follows the public scanner concepts
	// and names the unqualified proxy representation.
	auto const result{scan_contiguous_define(
		io_reserve_type<char_type, ::std::remove_cvref_t<T>>, buffer_begin, buffer_end, t)};
	if constexpr (!::fast_io::contiguous_scanner_result_in_range<char_type, T>)
	{
		if (!::fast_io::details::scan_iterator_in_current_chunk(buffer_begin, buffer_end, result.iter)) [[unlikely]]
		{
			// The conversion bridge owns only the materialized fragment. Validate before observing success so an escaped
			// iterator cannot be accepted merely because this path intentionally permits an unconsumed suffix. A scanner
			// that explicitly proves the closed-range contract has already discharged this check at its leaf boundary.
			throw_parse_code(parse_code::invalid);
		}
	}
	if (result.code != parse_code::ok)
	{
		throw_parse_code(result.code);
	}
}

template <::std::integral char_type, typename T, typename Arg>
inline constexpr void to_deal_with_contiguous_single_scatter(T &t, Arg &arg)
{
	basic_io_scatter_t<char_type> scatter{print_scatter_define(io_reserve_type<char_type, Arg>, arg)};
	if (scatter.len == 0u)
	{
		// Unlike the context bridge, the contiguous bridge invokes its scanner even for empty input. Supply one valid
		// object address so the CPO may compare `first == last` without being exposed to a null pointer pair. The scanner
		// receives an empty half-open range and therefore has no permission to inspect the dummy character.
		char_type dummy{};
		char_type const *const empty{__builtin_addressof(dummy)};
		deal_with_single_to<char_type>(empty, empty, t);
	}
	else
	{
		auto base{scatter.base};
		deal_with_single_to<char_type>(base, base + scatter.len, t);
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *to_impl_with_reserve_recursive(char_type *p, T &t, Args &...args)
{
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		p = copy_scatter(print_scatter_define(io_reserve_type<char_type, T>, t), p);
	}
	else
	{
		p = print_reserve_define(io_reserve_type<char_type, T>, p, t);
	}
	if constexpr (sizeof...(Args) == 0)
	{
		return p;
	}
	else
	{
		return to_impl_with_reserve_recursive<char_type>(p, args...);
	}
}

/// @brief Emits an all-static-reserve pack using the same representation that proved its aggregate capacity.
/// @details The dedicated path is a representation-coupling invariant, not merely an optimization.  A source may
///          independently expose both a static reserve writer and a repeatable scatter.  This branch computes storage
///          exclusively from the former, so allowing the generic scatter-first emitter to override that choice could
///          write beyond the proved range when the two protocols have different physical bounds.  Every recursive step
///          therefore uses `print_reserve_define`; the general run-time-sized branch below retains scatter-first policy
///          because its measurement function uses that exact same priority.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *to_impl_with_static_reserve_recursive(char_type *p, T &t, Args &...args)
{
	p = print_reserve_define(io_reserve_type<char_type, T>, p, t);
	if constexpr (sizeof...(Args) == 0u)
	{
		return p;
	}
	else
	{
		return to_impl_with_static_reserve_recursive<char_type>(p, args...);
	}
}

/// @brief Tests whether one normalized source can participate in terminal stack coalescing.
/// @details The source must separately authorize speculative formatting. Materialization then follows the same
///          representation priority as the mature contiguous `to` path: repeatable named scatter, type-level reserve,
///          or a dynamic reserve source with the destination-neutral non-fatal bounded-size protocol.
template <::std::integral char_type, typename T>
inline constexpr bool to_terminal_stack_component_v =
	::fast_io::eager_materialization_safe_printable<char_type, T> &&
	(::fast_io::details::to_repeatable_named_scatter_v<char_type, T> ||
	 ::fast_io::reserve_printable<char_type, T> ||
	 (::fast_io::dynamic_reserve_printable<char_type, T> &&
	  ::fast_io::single_pass_bounded_materialization_source<char_type, T>));

/// @brief Relation proof for one stack-only terminal contiguous execution plan.
template <typename char_type, typename Target, typename... Sources>
concept to_terminal_stack_candidate =
	::std::integral<char_type> && (sizeof...(Sources) > 1u) &&
	(::fast_io::details::to_small_stack_capacity<char_type>() != 0u) &&
	::fast_io::terminal_contiguous_context_scannable<char_type, Target> &&
	::fast_io::to_terminal_contiguous_staging_preferred_target<char_type, Target> &&
	(::fast_io::details::to_terminal_stack_component_v<char_type, Sources> && ...);

/// @brief Measures one eager-safe terminal component without materializing its reserve spelling.
/// @return `false` when this optional plan cannot fit; no parse result is represented here.
/// @details Scatter observation is captured into the caller's descriptor slot so the successful plan never calls its
///          CPO twice. Static reserve sizing is type-only, while a dynamic component uses only the destination-neutral
///          non-fatal bounded query. A rejected later component therefore discards at most pure measurements and
///          descriptors; it never causes an already-measured reserve formatter to run twice in the context fallback.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::to_terminal_stack_component_v<char_type, T>
inline constexpr bool to_terminal_stack_measure_one(
	::std::size_t remaining, ::std::size_t &bound,
	::fast_io::basic_io_scatter_t<char_type> &scatter, T &value)
{
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		scatter = print_scatter_define(io_reserve_type<char_type, T>, value);
		bound = scatter.len;
		if (remaining < bound)
		{
			return false;
		}
		return true;
	}
	else
	{
		if constexpr (::fast_io::reserve_printable<char_type, T>)
		{
			bound = print_reserve_size(io_reserve_type<char_type, T>);
		}
		else
		{
			bound = ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
				value, remaining);
		}
		if (bound == SIZE_MAX || remaining < bound)
		{
			return false;
		}
		return true;
	}
}

/// @brief Emits one already-admitted terminal component exactly once.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::to_terminal_stack_component_v<char_type, T>
inline constexpr char_type *to_terminal_stack_emit_one(
	char_type *current, ::std::size_t bound,
	::fast_io::basic_io_scatter_t<char_type> scatter, T &value)
{
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		return copy_scatter(scatter, current);
	}
	else
	{
		auto const component_end{bound == 0u ? current : current + bound};
		auto const actual_end{print_reserve_define(io_reserve_type<char_type, T>, current, value)};
		if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
				current, component_end, actual_end)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return actual_end;
	}
}

/// @brief Coalesces a complete eager-safe source pack into the fixed local buffer and scans it contiguously once.
/// @return `false` only when a source bound does not fit; scanner errors retain their ordinary exception behavior.
/// @details The public owner pack retains its by-value ABI; `Sources&...` only walks those stable owned objects and adds
///          no calling-convention requirement. This leaf deliberately remains ordinarily inline: unconditional cloning
///          can help selected small Clang shapes, but measured GCC fixed-P32 and stable-scatter shapes regress when the
///          complete measure/emit graph is forced into every caller. Placement is therefore an optimizer decision, while
///          ownership, representation coupling, and the single contiguous scan remain invariant.
template <::std::size_t capacity, ::std::integral char_type, typename Target, typename... Sources>
	requires ::fast_io::details::to_terminal_stack_candidate<char_type, Target, Sources...>
inline constexpr bool try_to_terminal_stack_contiguous(
	char_type (&buffer)[capacity], Target &target, Sources &...sources)
{
	::std::size_t bounds[sizeof...(Sources)]{};
	::fast_io::basic_io_scatter_t<char_type> scatters[sizeof...(Sources)]{};
	::std::size_t total{};
	::std::size_t index{};
	bool fits{true};
	auto const measure = [&]<typename Source>(Source &source)
	{
		if (!fits)
		{
			return;
		}
		fits = ::fast_io::details::to_terminal_stack_measure_one<char_type>(
			capacity - total, bounds[index], scatters[index], source);
		if (fits)
		{
			total += bounds[index];
		}
		++index;
	};
	(measure(sources), ...);
	if (!fits)
	{
		return false;
	}
	char_type *current{buffer};
	index = 0u;
	((current = ::fast_io::details::to_terminal_stack_emit_one<char_type>(
		  current, bounds[index], scatters[index], sources), ++index), ...);
	::fast_io::details::deal_with_single_to<char_type>(buffer, current, target);
	return true;
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_dynamic_reserve_size_with_scatter(
	[[maybe_unused]] T &t, Args &...args)
{
	::std::size_t res{};
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		// Emission selects the same repeatable named-scatter branch in `to_impl_with_reserve_recursive`. Measuring a
		// dynamic reserve representation here while emitting a scatter representation later can under-allocate even when
		// both protocols are individually valid.
		res = print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t).len;
	}
	else if constexpr (dynamic_reserve_printable<char_type, T>)
	{
		res = print_reserve_size(io_reserve_type<char_type, T>, t);
	}
	else if constexpr (reserve_printable<char_type, T>)
	{
		// This function is also used for packs mixing a static reserve producer with a dynamic one. The static capacity
		// remains part of the total even though it needs no object-dependent measurement.
		res = print_reserve_size(io_reserve_type<char_type, T>);
	}
	if constexpr (sizeof...(Args) == 0)
	{
		return res;
	}
	else
	{
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			res, calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...));
	}
}

/// @brief Proves that `to` can materialize its normalized print arguments without constructing a stream fallback.
/// @details This predicate is the type-level counterpart of the first branch in `basic_inplace_to_decay`.  Keeping the
///          complete fragment proof in one function prevents public availability from drifting away from execution:
///          context scanners may consume each fragment once, a single contiguous scatter is observed once, static
///          reserves need no replay, and a multi-fragment contiguous scan retains only explicitly repeatable scatters.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool inplace_to_direct_fragment_strategy_available() noexcept
{
	// An empty run or a non-scannable target cannot consume any direct printable fragment.
	if constexpr (
		sizeof...(Args) == 0u ||
		!(::fast_io::contiguous_scannable<char_type, T> ||
		  ::fast_io::context_scannable<char_type, T>))
	{
		return false;
	}
	else
	{
		constexpr bool all_named_fragments{
			((::fast_io::reserve_printable<char_type, Args> ||
			  ::fast_io::dynamic_reserve_printable<char_type, Args> ||
			  ::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_scatters{
			((::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_static_reserves{
			((::fast_io::reserve_printable<char_type, Args>) && ...)};
		constexpr bool context_fragment_strategy{
			::fast_io::context_scannable<char_type, T> &&
			(!(::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u))};
		constexpr bool contiguous_single_scatter_strategy{
			::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u && all_scatters};
		constexpr bool contiguous_two_pass_strategy{
			::fast_io::contiguous_scannable<char_type, T> &&
			((::fast_io::details::to_two_pass_fragment_available_v<char_type, Args>) && ...)};
		return all_named_fragments &&
			(context_fragment_strategy || contiguous_single_scatter_strategy ||
			 all_static_reserves || contiguous_two_pass_strategy);
	}
}

/// @brief Proves the exact dynamic-output fallback issued by `basic_inplace_to_decay`.
/// @details A context scanner formats one argument at a time so it can stop as soon as parsing completes; its proof must
///          therefore validate every singleton call.  A contiguous scanner formats the complete run in one call and
///          instead requires that exact pack.  Testing the concrete dynamic-buffer observer closes the former
///          character-only concept hole where a dummy-stream-only `print_define` was admitted and failed later inside
///          the dispatcher.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool inplace_to_dynamic_output_strategy_available() noexcept
{
	using output_type = ::fast_io::basic_dynamic_output_buffer_ref<
		::fast_io::basic_dynamic_output_buffer<char_type>>;
	// Context scanners require singleton printability because the fallback checks completion after every argument.
	if constexpr (
		::fast_io::context_scannable<char_type, T> &&
		(!(::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u)))
	{
		return (::fast_io::details::decay::print_freestanding_output_run_okay<
			false, output_type, Args>() && ...);
	}
	// Contiguous scanners observe one complete dynamic-buffer run and therefore validate the whole argument pack.
	else if constexpr (::fast_io::contiguous_scannable<char_type, T>)
	{
		return ::fast_io::details::decay::print_freestanding_output_run_okay<
			false, output_type, Args...>();
	}
	else
	{
		return false;
	}
}

/// @brief Accepts exactly the normalized source packs supported by either direct-fragment or dynamic-output conversion.
template <typename char_type, typename T, typename... Args>
concept inplace_to_decay_detect =
	::std::integral<char_type> &&
	(sizeof...(Args) != 0u &&
	 (::fast_io::contiguous_scannable<char_type, T> ||
	  ::fast_io::context_scannable<char_type, T>) &&
	 (::fast_io::details::inplace_to_direct_fragment_strategy_available<
		  char_type, T, Args...>() ||
	  ::fast_io::details::inplace_to_dynamic_output_strategy_available<
		  char_type, T, Args...>()));

} // namespace details

/// @brief Executes conversion by borrowing a target expression and an already-owned normalized source pack.
/// @details The caller owns every source for this complete call, while the forwarding target retains the category of a
///          possibly temporary scan alias.  This is the unique algorithm body used by both ownership front doors:
///          `basic_inplace_to_decay` owns public normalized prvalues, whereas value-returning `to` already owns them in
///          `basic_to_decay`.  Keeping the body reference-only prevents either front door from reconstructing an admitted
///          move-only proxy and makes all strategy CPO expressions the same named lvalues used by availability proofs.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to_decay_borrowed(T &&t, Args &...args)
{
	constexpr bool available{::fast_io::details::inplace_to_decay_detect<char_type, T, Args...>};
	// Instantiate execution only after the exact direct-or-dynamic strategy has been proved for the normalized pack.
	if constexpr (available)
	{
		// Native string-like destinations may opt into one complete append when every actual source spelling is
		// whitespace-free.  Check literal bytes before clearing the target; a rejected record therefore retains the
		// established scanner semantics and has no observable partial mutation.
		if constexpr (::fast_io::details::inplace_to_direct_printable<char_type, T, Args...>)
		{
			if (::fast_io::details::to_c_space_free_source_values<char_type>(args...) &&
				::fast_io::details::to_source_values_have_emitted_character<char_type>(args...) &&
				::fast_io::details::inplace_to_direct_source_values_safe<char_type>(t, args...))
			{
				inplace_to_direct_print_define(
					::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
					t, args...);
				return;
			}
		}
		constexpr bool direct_fragment_strategy{
			::fast_io::details::inplace_to_direct_fragment_strategy_available<
				char_type, T, Args...>()};
		constexpr bool all_scatters{
			((::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_static_reserves{((reserve_printable<char_type, Args>) && ...)};
		// Context scanning consumes every fragment immediately, a single contiguous scatter is observed once, and a
		// static-reserve pack needs no sizing pass. Every other contiguous composition is length-then-copy and therefore
		// enters it only when each selected scatter has explicit repeatable provenance (or a reserve fallback).
		if constexpr (direct_fragment_strategy)
		{
			constexpr bool no_need_dynamic_reserve{
				((reserve_printable<char_type, Args> ||
				  ::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
			if constexpr (context_scannable<char_type, T> &&
						  (!(contiguous_scannable<char_type, T> && sizeof...(args) == 1)))
			{
				constexpr bool terminal_stack_candidate{
					::fast_io::details::to_terminal_stack_candidate<char_type, T, Args...>};
				constexpr ::std::size_t small_stack_capacity{
					::fast_io::details::to_small_stack_capacity<char_type>()};
				constexpr bool bounded_fragment_stack_scratch_candidate{
					(::fast_io::details::to_bounded_fragment_stack_scratch_candidate_v<char_type, Args> || ...)};
				constexpr ::std::size_t bounded_fragment_stack_capacity{
					bounded_fragment_stack_scratch_candidate ? small_stack_capacity : 0u};
				// The terminal coalescer needs the complete operation-local array. The lazy bounded walker needs it only
				// when at least one source protocol can prove a write no larger than that array. If neither proof exists,
				// retain a one-element placeholder solely to keep the pointer expression well-formed; its zero template
				// capacity makes every materialization select the reusable heap owner, so the placeholder is never written.
				constexpr ::std::size_t stack_scratch_extent{
					(terminal_stack_candidate || bounded_fragment_stack_scratch_candidate) &&
						small_stack_capacity != 0u
						? small_stack_capacity
						: 1u};
				char_type stack_scratch[stack_scratch_extent];
				bool terminal_stack_completed{};
				if constexpr (terminal_stack_candidate)
				{
					terminal_stack_completed =
						::fast_io::details::try_to_terminal_stack_contiguous<small_stack_capacity, char_type>(
							stack_scratch, t, args...);
				}
				if (!terminal_stack_completed)
				{
					using state_type = ::fast_io::details::scan_context_state_t<char_type, T>;
					::fast_io::details::with_scan_context_state<state_type>([&](state_type &state) {
						if constexpr (all_scatters)
						{
							::fast_io::details::inplace_to_decay_buffer_scatter_context_impl<char_type>(
								state, t, args...);
						}
						else if constexpr (no_need_dynamic_reserve)
						{
							constexpr ::std::size_t maximum_reserve_size{
								::fast_io::details::calculate_print_normal_maxium_size<char_type, false, Args...>()};
							if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
									maximum_reserve_size, char_type>)
							{
								// One reusable fragment buffer fits the configured hot-stack budget.
								char_type buffer[maximum_reserve_size];
								::fast_io::details::inplace_to_decay_buffer_context_impl<char_type>(
									buffer, state, t, args...);
							}
							else
							{
								// A type-level reserve bound is a capacity proof, not permission to enlarge every caller's
								// frame. The dynamic branch preserves reuse once the policy limit is exceeded.
								::fast_io::details::local_operator_new_array_ptr<char_type> buffer(maximum_reserve_size);
								::fast_io::details::inplace_to_decay_buffer_context_impl<char_type>(
									buffer.ptr, state, t, args...);
							}
						}
						else
						{
								// This owner outlives every fragment transition but not the conversion. Each reached source is
							// measured exactly once; an early `ok` therefore still prevents all suffix observations. The initial
							// heap floor matches the operation-local stack cap, amortizing small unhinted dynamic fragments without
							// reserving that cap in the caller's frame.
							::fast_io::details::local_operator_new_array_ptr<char_type> heap_scratch;
							::fast_io::details::inplace_to_decay_bounded_buffer_context_impl<
								bounded_fragment_stack_capacity, small_stack_capacity, char_type>(
								stack_scratch, heap_scratch, state, t, args...);
						}
					});
				}
			}
			else if constexpr (contiguous_scannable<char_type, T>)
			{
				if constexpr (all_scatters && sizeof...(Args) == 1) // crucial for performance
				{
					::fast_io::details::to_deal_with_contiguous_single_scatter<char_type>(t, args...);
				}
				else if constexpr (all_static_reserves)
				{
					constexpr ::std::size_t total_size{
						::fast_io::details::decay::calculate_scatter_reserve_size<char_type, Args...>()};
					if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
							total_size, char_type>)
					{
						// A zero bound still needs one addressable object for the valid empty half-open scanner range.
						char_type buffer[total_size == 0u ? 1u : total_size];
						auto const ret{
							::fast_io::details::to_impl_with_static_reserve_recursive(buffer, args...)};
						::fast_io::details::deal_with_single_to<char_type>(buffer, ret, t);
					}
					else
					{
						// Summing several individually valid reserve bounds can still create an unbounded automatic
						// object. Dynamic storage keeps one materialization without coupling capacity to frame size.
						::fast_io::details::local_operator_new_array_ptr<char_type> buffer(total_size);
						auto const ret{::fast_io::details::to_impl_with_static_reserve_recursive(
							buffer.ptr, args...)};
						::fast_io::details::deal_with_single_to<char_type>(buffer.ptr, ret, t);
					}
				}
				else
				{
					::std::size_t const maximum_reserve_size{
						::fast_io::details::calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...)};
					::fast_io::details::local_operator_new_array_ptr<char_type> heap_buffer(maximum_reserve_size);
					auto ret{::fast_io::details::to_impl_with_reserve_recursive(heap_buffer.ptr, args...)};
					::fast_io::details::deal_with_single_to<char_type>(heap_buffer.ptr, ret, t);
				}
			}
		}
		else
		{
			static_assert(
				::fast_io::details::inplace_to_dynamic_output_strategy_available<
					char_type, T, Args...>(),
				"the normalized to() fallback is not printable to its dynamic output buffer");
			basic_dynamic_output_buffer<char_type> buffer;
			decltype(auto) ref = ::fast_io::operations::output_stream_ref(buffer);
			if constexpr (context_scannable<char_type, T> &&
						  (!(contiguous_scannable<char_type, T> && sizeof...(args) == 1)))
			{
				using state_type = ::fast_io::details::scan_context_state_t<char_type, T>;
				::fast_io::details::with_scan_context_state<state_type>([&](state_type &state) {
					::fast_io::details::inplace_to_decay_context_impl(ref, state, t, args...);
				});
			}
			else if constexpr (contiguous_scannable<char_type, T>)
			{
				// The caller already owns every normalized source proxy. Continue through the stable-reference implementation
				// rather than the public by-value ownership shim: admission models these exact named lvalues,
				// and copying them here would both reject a move-only alias and add one complete O(N) construction layer.
				::fast_io::operations::decay::print_freestanding_decay_impl<false>(ref, args...);
				// The dynamic output object may have grown from its inline array, so use its active pointers rather than
				// naming the embedded storage. Both pointers are updated together by every growth operation.
				::fast_io::details::deal_with_single_to<char_type>(buffer.begin_ptr, buffer.curr_ptr, t);
			}
			else
			{
				constexpr bool type_error{context_scannable<char_type, T>};
				static_assert(type_error, "scan type error");
			}
		}
	}
	else
	{
		static_assert(available, "either some arguments are not printable or the target is not scannable");
	}
}

/// @brief Owns a normalized scan target and source proxy prvalues supplied by the public inplace conversion boundary.
/// @details This is the value-decay ABI boundary: small trivial target proxies retain the platform's ordinary
///          register/stack argument classification, while guaranteed parameter initialization owns prvalues for the
///          complete borrowed algorithm call. Value-returning `to` already owns its target locally and therefore enters
///          the borrowed body directly, avoiding an extra target construction and an extra source-pack copy layer.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to_decay(T t, Args... args)
{
	::fast_io::basic_inplace_to_decay_borrowed<char_type>(t, args...);
}

/// @brief Owns normalized source values while retaining an existing scan-alias lvalue by identity.
/// @details A scan customization may deliberately return a stable, noncopyable lvalue proxy. Such a result cannot pass
///          through the value-decay target entry above, but it also needs no lifetime extension: the public target owns
///          it for the complete synchronous conversion. Keeping this case in a separately named entry prevents its
///          reference ABI from replacing value transport for the common prvalue target while preserving the exact
///          lvalue selected by the alias CPO.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to_decay_borrowed_target(T &t, Args... args)
{
	::fast_io::basic_inplace_to_decay_borrowed<char_type>(t, args...);
}

namespace details
{

template <::std::integral char_type, typename T, typename... Args>
	requires ::fast_io::details::inplace_to_decay_detect<char_type, T, Args...>
inline constexpr void basic_inplace_to_decay_model(T &&, Args &&...)
{
}

template <typename char_type, typename T, typename... Args>
concept can_do_inplace_to = requires(T &t, Args &&...args) {
	::fast_io::details::basic_inplace_to_decay_model<char_type>(
		::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(t)),
		io_print_forward<char_type>(io_print_alias(args))...);
};

template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_source_replacement_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_replacement_t<char_type, T>;

/// The exact already-normalized prvalue passed by the compiler-constant true arm.
/// `plain_true_forward` intentionally treats candidates and untouched sources differently: a candidate aliases its
/// newly materialized prvalue, while a non-candidate aliases the helper's named source lvalue. Re-running
/// `io_print_alias` on the raw replacement type would erase that distinction and make availability disagree with the
/// call below for ref-qualified customization sets.
template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_normalized_t = ::std::remove_cvref_t<decltype(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_plain_true_forward<
			false, char_type>(::std::declval<T>()))>;

/// The exact forwarding-parameter type deduced by `basic_inplace_to_decay` for the named public scan target.
template <::std::integral char_type, typename T>
using inplace_to_normalized_target_t = decltype(
	::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(
		::std::declval<::std::remove_reference_t<T> &>())));

/// The exact normalized source type passed by the historical false arm.
template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_source_normalized_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_normalized_t<
			char_type, false, T>;

/// @brief Proves that this compiler has an audited, formatter-free `to` true arm.
/// @details GCC 11 and later erase the complete proxy graph when immutable fragment slots are expanded at compile
///          time. Clang 21 and later instead erase the precise proxy writer. Clang 13--20 retain 967--1,078
///          instructions of floating proxy code for the same literal, and native MSVC has no builtin query, so those
///          implementations are rejected before a replacement type or value query is formed. The positive bounds are
///          deliberately future-open; every newly observed reversal must narrow this proof before the query can run.
inline consteval bool inplace_to_compiler_constant_codegen_supported() noexcept
{
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
	return true;
#else
	return false;
#endif
}

/// @brief Computes the aggregate type-level reserve bound of a proven replacement run.
/// @details This is a capacity proof only. The GCC emitter below obtains bytes from immutable fragments, while the
///          Clang emitter uses exact writers; both spellings are required by their provider contracts to equal the
///          ordinary reserve spelling and therefore cannot exceed this bound.
template <::std::integral char_type, typename... Args>
inline consteval ::std::size_t
inplace_to_compiler_constant_reserve_capacity() noexcept
{
	constexpr ::std::size_t maximum{
		::fast_io::details::compiler_constant_materialization_max_bytes /
		sizeof(char_type)};
	::std::size_t total{};
	((total = [](::std::size_t current) constexpr {
		constexpr ::std::size_t extent{print_reserve_size(
			::fast_io::io_reserve_type<char_type,
				::std::remove_cvref_t<Args>>)};
		return current > maximum || extent > maximum - current
			? SIZE_MAX
			: current + extent;
	}(total)), ...);
	return total;
}

/// @brief Admits only replacement packs whose selected compiler strategy is bounded and completely specified.
/// @details Every source must be a candidate, so this true arm never changes a passive sibling's normalization or
///          replay policy. The target is the exact normalized contiguous scanner used by `basic_to_decay`. GCC needs
///          a bounded immutable-fragment protocol because its 11/12 precise writer leaves a run-time decimal loop;
///          Clang needs the exact compact protocol because its pre-21 fragment graph survives optimization. A maximum
///          of 64 fragment slots and 256 bytes bounds both compile-time work and any pre-optimization automatic state.
template <::std::integral char_type, typename T, typename... SourceArgs>
inline consteval bool
inplace_to_compiler_constant_direct_strategy_available() noexcept
{
	if constexpr (!::fast_io::details::
		inplace_to_compiler_constant_codegen_supported())
	{
		return false;
	}
	else if constexpr (
		sizeof...(SourceArgs) == 0u || sizeof...(SourceArgs) > 16u ||
		!(::fast_io::operations::decay::
			  print_compiler_constant_pre_normalization_candidate_v<
				  char_type, SourceArgs> && ...))
	{
		return false;
	}
	else
	{
		using target_type =
			::fast_io::details::inplace_to_normalized_target_t<char_type, T>;
		if constexpr (!::fast_io::contiguous_scannable<char_type, target_type>)
		{
			return false;
		}
		else if constexpr (!(::fast_io::reserve_printable<
							 char_type,
							 ::fast_io::details::
								 inplace_to_compiler_constant_normalized_t<
									 char_type, SourceArgs>> && ...))
		{
			return false;
		}
		else
		{
			constexpr ::std::size_t capacity{
				::fast_io::details::
					inplace_to_compiler_constant_reserve_capacity<
						char_type,
						::fast_io::details::
							inplace_to_compiler_constant_normalized_t<
								char_type, SourceArgs>...>()};
			if constexpr (
				capacity == SIZE_MAX ||
				capacity > ::fast_io::details::
					compiler_constant_materialization_max_bytes /
						sizeof(char_type))
			{
				return false;
			}
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
			else if constexpr (!((
				::fast_io::compiler_constant_expanded_fragment_preferred<
					char_type,
					::fast_io::details::
						inplace_to_compiler_constant_normalized_t<
							char_type, SourceArgs>> ||
				::fast_io::compiler_constant_precise_compact_preferred<
					char_type,
					::fast_io::details::
						inplace_to_compiler_constant_normalized_t<
							char_type, SourceArgs>>) && ...))
			{
				return false;
			}
			else
			{
				constexpr ::std::size_t fragment_count{[]() consteval {
					::std::size_t total{};
					((total = [](::std::size_t current) constexpr {
						using replacement_type = ::fast_io::details::
							inplace_to_compiler_constant_normalized_t<
								char_type, SourceArgs>;
						constexpr ::std::size_t extent{[]() constexpr {
							if constexpr (::fast_io::
								compiler_constant_expanded_fragment_preferred<
									char_type, replacement_type>)
							{
								return print_compiler_constant_static_fragments_size(
									::fast_io::io_reserve_type<
										char_type, replacement_type>);
							}
							else
							{
								return static_cast<::std::size_t>(0u);
							}
						}()};
						return current > 64u || extent > 64u - current
							? SIZE_MAX
							: current + extent;
					}(total)), ...);
					return total;
				}()};
				return fragment_count != SIZE_MAX && fragment_count <= 64u;
			}
#elif defined(__clang__) && 21 <= __clang_major__
			else
			{
				return (::fast_io::compiler_constant_precise_compact_preferred<
					char_type,
					::fast_io::details::
						inplace_to_compiler_constant_normalized_t<
							char_type, SourceArgs>> && ...);
			}
#else
			else
			{
				return false;
			}
#endif
		}
	}
}

/// @brief Detects a status owner which the selected dynamic-output `to` strategy would actually invoke.
/// @details Direct fragment conversion never enters an output dispatcher, so an otherwise matching status CPO is
///          irrelevant there. The dynamic context fallback emits singleton runs to preserve early completion, whereas
///          the contiguous fallback emits the complete pack. Mirroring that exact shape prevents a compiler-constant
///          replacement from adding or removing a whole-run customization and thereby changing the characters scanned.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool
inplace_to_selected_dynamic_status_owner() noexcept
{
	// Direct fragment scanning bypasses every output dispatcher, so no dynamic-output status owner can participate.
	if constexpr (
		::fast_io::details::inplace_to_direct_fragment_strategy_available<
			char_type, T, Args...>())
	{
		return false;
	}
	else
	{
		using output_type = ::fast_io::basic_dynamic_output_buffer_ref<
			::fast_io::basic_dynamic_output_buffer<char_type>>;
		// A fragmented context fallback dispatches each argument separately to preserve early completion.
		if constexpr (
			::fast_io::context_scannable<char_type, T> &&
			(!(::fast_io::contiguous_scannable<char_type, T> &&
			   sizeof...(Args) == 1u)))
		{
			return (false || ... ||
				::fast_io::operations::decay::defines::
					has_status_print_define<false, output_type, Args>);
		}
		// A contiguous fallback dispatches the complete pack and must test its whole-run status customization.
		else if constexpr (::fast_io::contiguous_scannable<char_type, T>)
		{
			return ::fast_io::operations::decay::defines::
				has_status_print_define<false, output_type, Args...>;
		}
		else
		{
			return false;
		}
	}
}

/// @brief Proves that `to` may replace one or more public source values before print normalization.
/// @details The replacement uses the same destination-neutral compiler-constant CPO as print and concat, but admission
///          is checked against `to`'s exact scanner and dynamic-output/direct-fragment strategies. Semantic nodes keep
///          their existing graph-owned normalization. Both arms perform the same per-source alias/status forwarding;
///          destination status owners are checked separately against the exact strategy and run shape below. Only
///          candidate proxy state counts toward the common materialization budget; unchanged run-time fragments retain
///          their ordinary zero-copy or reserve protocol.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool
inplace_to_compiler_constant_source_available() noexcept
{
	constexpr bool has_candidate{
		(false || ... ||
		 ::fast_io::operations::decay::
			 print_compiler_constant_pre_normalization_candidate_v<
				 char_type, Args>)};
	constexpr bool has_semantic_source{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 char_type, Args>)};
	// Reject unsupported compilers before forming a replacement type. This ordering is part of the code-generation
	// proof: a fail-closed call must not instantiate a proxy whose formatter could remain reachable or trigger a
	// compiler-specific frontend defect.
	if constexpr (
		!has_candidate || has_semantic_source ||
		!::fast_io::details::
			inplace_to_compiler_constant_codegen_supported())
	{
		return false;
	}
	else
	{
		constexpr bool has_semantic_replacement{
			(false || ... ||
			 ::fast_io::details::decay::print_semantic_input_argument_v<
				 char_type,
				 ::fast_io::details::inplace_to_compiler_constant_source_replacement_t<
					 char_type, Args>>)};
		// Semantic replacements retain their graph-owned condition/pack traversal and therefore never enter this flat arm.
		if constexpr (has_semantic_replacement)
		{
			return false;
		}
		else
		{
		using target_type =
			::fast_io::details::inplace_to_normalized_target_t<char_type, T>;
		constexpr bool source_status_owner{
			::fast_io::details::inplace_to_selected_dynamic_status_owner<
				char_type, target_type,
				::fast_io::details::
					inplace_to_compiler_constant_source_normalized_t<
						char_type, Args>...>()};
		constexpr bool replacement_status_owner{
			::fast_io::details::inplace_to_selected_dynamic_status_owner<
				char_type, target_type,
				::fast_io::details::inplace_to_compiler_constant_normalized_t<
					char_type, Args>...>()};
		// Either spelling must retain a selected dynamic-output status owner, so direct replacement is rejected.
		if constexpr (source_status_owner || replacement_status_owner)
		{
			return false;
		}

		constexpr ::std::size_t proxy_bytes{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes};
			::std::size_t total{};
			((total = [](::std::size_t current) consteval {
				// Only actual candidates contribute proxy state; untouched sources retain their zero-byte budget entry.
				if constexpr (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_candidate_v<
						char_type, Args>)
				{
					constexpr ::std::size_t extent{sizeof(
						::fast_io::details::inplace_to_compiler_constant_source_replacement_t<
							char_type, Args>)};
					return current > maximum || extent > maximum - current
						? SIZE_MAX
						: current + extent;
				}
				else
				{
					return current;
				}
			}(total)),
			 ...);
			return total;
		}()};
		return proxy_bytes != SIZE_MAX &&
			   proxy_bytes <=
				   ::fast_io::details::compiler_constant_materialization_max_bytes &&
			   ::fast_io::details::
				   inplace_to_compiler_constant_direct_strategy_available<
					   char_type, T, Args...>() &&
			   ::fast_io::details::inplace_to_decay_detect<
				   char_type,
				   target_type,
				   ::fast_io::details::inplace_to_compiler_constant_normalized_t<
					   char_type, Args>...>;
		}
	}
}

/// @brief Copies a type-bounded immutable-fragment array without a run-time descriptor loop.
/// @details GCC 11/12 leave both a descriptor loop and calls to `memcpy` when the returned fragment prefix is traversed
///          dynamically. Expanding every provider-declared slot makes unused zero-initialized entries disappear and
///          reduces the `3.125` probe to five immediate byte stores on every tested GCC 11--17. This helper is reachable
///          only after the successful source query and the 64-slot/256-byte availability proof.
template <::std::integral char_type, ::std::size_t... index>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *
inplace_to_compiler_constant_copy_fragment_slots(
	char_type *current,
	::fast_io::basic_io_scatter_t<char_type> const *fragments,
	::std::index_sequence<index...>) noexcept
{
	((current = fragments[index].len == 0u
		? current
		: ::fast_io::freestanding::non_overlapped_copy_n(
			fragments[index].base, fragments[index].len, current)), ...);
	return current;
}

/// @brief Emits one already-proven replacement using the compiler-specific deletion proof.
/// @details GCC expands immutable fragment slots because its pre-13 precise decimal writer retains a loop. Clang 21+
///          uses the provider's exact-size protocol because its fragment graph is the code-generation reversal. Neither
///          branch names an ordinary native formatter, and both operate only on a replacement proxy created after the
///          value query succeeded.
template <::std::integral char_type, typename Arg>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *
inplace_to_compiler_constant_emit_one(char_type *current, Arg &arg) noexcept
{
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
	using arg_type = ::std::remove_cvref_t<Arg>;
	if constexpr (::fast_io::compiler_constant_expanded_fragment_preferred<
		char_type, arg_type>)
	{
		constexpr ::std::size_t fragment_count{
			print_compiler_constant_static_fragments_size(
				::fast_io::io_reserve_type<char_type, arg_type>)};
		::fast_io::basic_io_scatter_t<char_type> fragments[fragment_count]{};
		(void)print_compiler_constant_static_fragments_define(
			::fast_io::io_reserve_type<char_type, arg_type>, fragments, arg);
		return ::fast_io::details::
			inplace_to_compiler_constant_copy_fragment_slots(
				current, fragments,
				::std::make_index_sequence<fragment_count>{});
	}
	else
	{
		::std::size_t const precise_size{print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, arg_type>, arg)};
		char_type *const expected_end{current + precise_size};
		char_type *const actual_end{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, current,
			precise_size, arg)};
		if (actual_end != expected_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return expected_end;
	}
#elif defined(__clang__) && 21 <= __clang_major__
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const precise_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, arg_type>, arg)};
	char_type *const expected_end{current + precise_size};
	char_type *const actual_end{print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, arg_type>, current,
		precise_size, arg)};
	if (actual_end != expected_end) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return expected_end;
#else
	// Availability rejects this instantiation before any value query on an unaudited compiler.
	(void)arg;
	return current;
#endif
}

/// @brief Keeps the floating scanner out of the GCC 11/12 literal-formatting frame.
/// @details After formatting has become five immediate stores, GCC 11 and 12 otherwise inline the independent decimal
///          scanner into the caller, growing the `to<double>(3.125)` root to 1,890 and 1,627 instructions. This
///          26-instruction caller boundary affects only the already-selected true arm; the ordinary run-time `to` path
///          continues to call its historical scanner directly.
template <::std::integral char_type, typename T>
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 12
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
#endif
inline constexpr void scan_contiguous_materialized_to(
	char_type const *first, char_type const *last, T &target)
{
	::fast_io::details::deal_with_single_to<char_type>(
		first, last, target);
}

/// @brief Formats a proven replacement pack once and presents only its completed character range to the scanner.
/// @details The type-level reserve sum bounds the local range; every provider's fragment or exact protocol promises
///          byte-for-byte equivalence with its ordinary spelling. Forced placement is confined to this true-only
///          formatter. The scanner remains a separate operation, and an endpoint mismatch terminates rather than
///          re-entering a run-time formatter after a successful builtin query. Forwarding references are a lifetime-
///          neutral borrow here: inplace conversion passes replacement prvalues whose full expression contains this
///          call, while value-returning conversion passes objects already owned by its source-materialization frame.
///          A by-value pack would unnecessarily copy the latter and impose an unstated copyability requirement.
template <::std::integral char_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void inplace_to_compiler_constant_direct_materialized(
	T &&target, Args &&...args)
{
	constexpr ::std::size_t capacity{
		::fast_io::details::inplace_to_compiler_constant_reserve_capacity<
			char_type, Args...>()};
	static_assert(
		capacity != SIZE_MAX &&
		capacity <= ::fast_io::details::
			compiler_constant_materialization_max_bytes / sizeof(char_type));
	char_type buffer[capacity == 0u ? 1u : capacity];
	char_type *current{buffer};
	((current = ::fast_io::details::
		  inplace_to_compiler_constant_emit_one<char_type>(current, args)), ...);
	::fast_io::details::scan_contiguous_materialized_to<char_type>(
		buffer, current, target);
}

/// @brief Executes the proven inplace conversion after all public source values have passed the successful query.
/// @details Each source becomes an owned proxy prvalue before this helper enters the direct formatter. The availability
///          proof admits no passive sibling, so no run-time source is copied, replayed, or observed through a different
///          alias category.
template <::std::integral char_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void inplace_to_compiler_constant_source_materialized(
	T &&target, Args &&...args)
{
	::fast_io::details::inplace_to_compiler_constant_direct_materialized<
		char_type>(
			::fast_io::io_scan_forward<char_type>(
				::fast_io::io_scan_alias(target)),
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_plain_true_forward<
					false, char_type>(::std::forward<Args>(args))...);
}

/// @brief Constructs a value target only after its public sources have been normalized into proven replacements.
/// @details Function arguments are materialized before entry, preserving `to`'s historical order in which source alias
///          operations precede target construction. Scalar initialization and non-scalar default initialization match
///          `basic_to_decay`; only the formatting continuation is replaced.
template <::std::integral char_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr T to_compiler_constant_source_materialized(Args... args)
{
	if constexpr (::std::is_scalar_v<T>)
	{
		T value{};
		::fast_io::details::inplace_to_compiler_constant_direct_materialized<
			char_type>(
				::fast_io::io_scan_forward<char_type>(
					::fast_io::io_scan_alias(value)),
				args...);
		return value;
	}
	else
	{
		T value;
		::fast_io::details::inplace_to_compiler_constant_direct_materialized<
			char_type>(
				::fast_io::io_scan_forward<char_type>(
					::fast_io::io_scan_alias(value)),
				args...);
		return value;
	}
}

/// @brief Shared public source boundary for every character-domain `inplace_to` facade.
/// @details The false arm is exactly the historical alias/forward/decay expression. Consequently an unknown value pays
///          no proxy construction, size query, or extra output pass; only a proven true optimizer query enters the
///          optional replacement arm. Forbidden raw output sources are diagnosed here before either strategy is formed.
template <::std::integral char_type, typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// GCC 11--17 otherwise outline this source-query boundary for an existing target. A literal dynamic-precision source
// then reaches the same native formatter graph as its unknown peer, while forcing this forwarding-only edge lets the
// already-proved materializer erase that graph; the unknown-value continuation remains the historical decay call.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void inplace_to_compiler_constant_checked_entry(
	T &&target, Args &&...args)
{
	constexpr bool has_raw_source{
		::fast_io::details::has_raw_print_arg<Args...>};
	constexpr bool ordinary_available{
		!has_raw_source &&
		::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
	// Keep malformed public source/target combinations in the diagnostic arm without instantiating either execution path.
	if constexpr (has_raw_source)
	{
		// Every public character-domain facade shares the IO-level raw-source diagnostic.
		::fast_io::details::print_raw_static_assert<Args...>();
	}
	else if constexpr (ordinary_available)
	{
		// Enter the optional source replacement only when its normalized scan and status semantics have been proved.
		if constexpr (
			::fast_io::details::inplace_to_compiler_constant_source_available<
				char_type, T, Args &&...>())
		{
			if (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_gate<char_type>(args...))
			{
				::fast_io::details::inplace_to_compiler_constant_source_materialized<
					char_type>(::std::forward<T>(target),
							   ::std::forward<Args>(args)...);
				return;
			}
		}
		using normalized_target_expression = decltype(
			::fast_io::io_scan_forward<char_type>(
				::fast_io::io_scan_alias(target)));
		if constexpr (::std::is_lvalue_reference_v<normalized_target_expression>)
		{
			// A customization-authored lvalue is already owned by the public target. Borrow that exact object, but keep
			// the source pack's ordinary by-value decay boundary.
			::fast_io::basic_inplace_to_decay_borrowed_target<char_type>(
				::fast_io::io_scan_forward<char_type>(
					::fast_io::io_scan_alias(target)),
				::fast_io::io_print_forward<char_type>(
					::fast_io::io_print_alias(args))...);
		}
		else
		{
			// A normalized prvalue is initialized directly into the decayed parameter, preserving both ownership and
			// the target ABI's value classification without an intervening reference parameter.
			::fast_io::basic_inplace_to_decay<char_type>(
				::fast_io::io_scan_forward<char_type>(
					::fast_io::io_scan_alias(target)),
				::fast_io::io_print_forward<char_type>(
					::fast_io::io_print_alias(args))...);
		}
	}
	else
	{
		static_assert(ordinary_available,
					  "either some arguments are not printable or the target is not scannable");
	}
}

} // namespace details

inline namespace io
{

/// @brief Applies the shared compiler-constant source gate to an explicit character-domain inplace conversion.
template <::std::integral char_type, typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This character-domain forwarding edge is jointly required with the checked boundary below: leaving either outlined
// hides the public source value from GCC's builtin query. It contains no formatting strategy or target-side mutation.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_inplace_to(T &&t, Args &&...args)
{
	::fast_io::details::inplace_to_compiler_constant_checked_entry<char_type>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable narrow-character fragments into an existing scan target.
template <typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// Final narrow-character source-expression edge for GCC's inplace conversion query. The constant arm becomes
// formatter-free on GCC 11--17, and the unknown arm retains the ordinary inplace conversion graph.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable wide-character fragments into an existing scan target.
template <typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This is the wchar_t instantiation of the same forwarding-only public edge proved for `inplace_to`; without forced
// placement GCC 11--17 may move the builtin query below an opaque parameter boundary. No formatter is selected here.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void winplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<wchar_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-8 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This is the char8_t instantiation of the measured public source edge. The attribute exposes the original expression
// to the IO-level gate only; the successful and unknown-value formatting strategies remain owned by that gate.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void u8inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char8_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-16 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This char16_t facade has the same single forwarding expression as the narrow measured edge. Forced placement is
// needed to prevent GCC's parameter boundary from making every source appear unknown to the builtin query.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void u16inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char16_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-32 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This char32_t facade is a type-domain projection of the measured edge and contains no algorithmic work. Inlining it
// preserves the caller expression for the shared query while the false arm remains the ordinary runtime conversion.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void u32inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char32_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

} // namespace io

namespace decay
{

/// @brief Constructs and scans a target from an already-normalized source pack without repeating alias normalization.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr T basic_to_decay(Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		return T();
	}
	else
	{
		// `Args...` have already passed the public source alias/forward boundary and are owned by value here. Re-applying
		// `can_do_inplace_to` would alias those normalized objects a second time, so a non-idempotent alias set could make
		// this admission test disagree with the direct `args...` expression in the body. Model precisely that body instead.
		constexpr bool available{
			::fast_io::details::inplace_to_decay_detect<
				char_type,
				::fast_io::details::inplace_to_normalized_target_t<char_type, T>,
				Args...>};
		// Construct the target only when the already-normalized source pack has a concrete conversion strategy.
		if constexpr (available)
		{
			// A single source-proved lexical token and a target-proved range constructor need no incremental delimiter
			// search. Both proof CPOs are required: arbitrary printables may contain spaces, while arbitrary strlike scan
			// targets may assign semantics other than the built-in string token grammar.
			if constexpr (
				sizeof...(Args) == 1u &&
				::fast_io::c_space_free_fragment_constructible_scan_target<char_type, T> &&
				(::fast_io::c_space_free_print_fragment<char_type, Args> && ...))
			{
				return ::fast_io::details::decay::basic_general_concat_phase1_decay_ref_impl<
					false, char_type, T>(args...);
			}
			// Scalar targets are value-initialized so a scanner can safely assign only the fields required by its protocol.
			else if constexpr (::std::is_scalar_v<T>)
			{
				T v{};
				basic_inplace_to_decay_borrowed<char_type>(
					::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(v)), args...);
				return v;
			}
			else
			{
				T v;
				basic_inplace_to_decay_borrowed<char_type>(
					::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(v)), args...);
				return v;
			}
		}
		else
		{
			static_assert(available,
				"the normalized arguments are not printable to the to() conversion strategy");
			return T();
		}
	}
}

} // namespace decay

namespace details
{

/// @brief Shared compiler-constant source boundary for every value-returning `to` facade.
/// @details Both arms normalize public print sources before constructing the scan target. The successful query enters
///          the bounded formatter-free materializer; the false arm remains the historical `basic_to_decay` expression.
///          This preserves the observable order in which source alias CPO side effects precede `T`'s construction.
///          Forbidden raw output sources use the same Core diagnostic as the stream-oriented IO facades.
template <::std::integral char_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// The builtin query must observe the public source expression. With ordinary placement GCC 15 outlines this boundary,
// so both literal and unknown callers jump to an opaque parameter and the literal cannot enter the proven true arm.
// Forcing this edge alone is text-neutral and merely moves the boundary to `basic_to`; the paired facade edge below is
// therefore required. The unknown-value continuation remains the unchanged `basic_to_decay` expression.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr T to_compiler_constant_checked_entry(Args &&...args)
{
	// An empty conversion has no source boundary and preserves the historical default-construction semantics directly.
	if constexpr (sizeof...(Args) == 0u)
	{
		// `basic_to_decay` has always defined the empty conversion as ordinary value/default construction. Keep the public
		// source boundary transparent for that case: there is no source protocol to normalize or compiler-constant query.
		return ::fast_io::decay::basic_to_decay<char_type, T>();
	}
	else
	{
		constexpr bool has_raw_source{
			::fast_io::details::has_raw_print_arg<Args...>};
		constexpr bool ordinary_available{
			!has_raw_source &&
			::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
		// Delay all conversion instantiation until the public alias and scanner protocols are known to be valid.
		if constexpr (has_raw_source)
		{
			// Match inplace_to and the stream facades before forming any print/scan strategy.
			::fast_io::details::print_raw_static_assert<Args...>();
		}
		else if constexpr (ordinary_available)
		{
			// The compiler-constant arm is available only when replacing sources preserves the exact selected scan strategy.
			if constexpr (
				::fast_io::details::inplace_to_compiler_constant_source_available<
					char_type, T, Args &&...>())
			{
				if (::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_gate<char_type>(args...))
				{
					return ::fast_io::details::
						to_compiler_constant_source_materialized<
							char_type, T>(
							::fast_io::operations::decay::
								print_compiler_constant_pre_normalization_plain_true_forward<
									false, char_type>(::std::forward<Args>(args))...);
				}
			}
			return ::fast_io::decay::basic_to_decay<char_type, T>(
				::fast_io::io_print_forward<char_type>(
					::fast_io::io_print_alias(args))...);
		}
		else
		{
			static_assert(ordinary_available,
						  "either some arguments are not printable or the target is not scannable");
		}
	}
}

} // namespace details

inline namespace io
{

/// @brief Constructs a target in the requested character domain through the shared compiler-constant source boundary.
template <::std::integral char_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// Paired source-query facade edge. Deleting it after forcing the checked entry leaves GCC 15 callers as a two-
// instruction jump to `basic_to` and again hides constants from `__builtin_constant_p`. It contains no formatting
// policy and only forwards the exact public expression into the IO-level availability/query boundary.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr T basic_to(Args &&...args)
{
	return ::fast_io::details::to_compiler_constant_checked_entry<char_type, T>(
		::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning narrow-character source fragments.
template <typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// Final source-expression edge. Without it GCC 15 leaves the literal caller as a jump to `to`, so the paired IO
// boundaries below still receive an opaque parameter. The same placement is applied to every character-domain facade;
// no facade makes a formatting decision, and an unknown source continues into the unchanged false arm.
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr T to(Args &&...args)
{
	return ::fast_io::basic_to<char, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning wide-character source fragments.
template <typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// The wchar_t facade is the same forwarding-only edge as `to`; the character type changes only the downstream
// instantiation. Forced placement preserves the public expression for the shared query and adds no format policy.
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr T wto(Args &&...args)
{
	return ::fast_io::basic_to<wchar_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-8 code-unit source fragments.
template <typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// The char8_t facade contains only the measured forwarding expression. Keeping that expression in the caller prevents
// the GCC 11--17 and Clang 21--23 query boundary from degrading a literal into an opaque function parameter.
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr T u8to(Args &&...args)
{
	return ::fast_io::basic_to<char8_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-16 code-unit source fragments.
template <typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// The char16_t facade is structurally identical to the measured narrow edge and performs no formatting itself. Forced
// placement exposes only the original source expression; the IO-level gate still owns both result strategies.
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr T u16to(Args &&...args)
{
	return ::fast_io::basic_to<char16_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-32 code-unit source fragments.
template <typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// The char32_t facade is the final forwarding-only projection of the measured source edge. Inlining avoids an opaque
// parameter at the builtin query without duplicating or changing either the constant or runtime formatter graph.
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr T u32to(Args &&...args)
{
	return ::fast_io::basic_to<char32_t, T>(::std::forward<Args>(args)...);
}

} // namespace io

} // namespace fast_io

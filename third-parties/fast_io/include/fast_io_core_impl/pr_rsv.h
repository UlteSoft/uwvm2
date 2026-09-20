#pragma once

namespace fast_io
{

namespace details
{

/// @brief Named-expression category used after a forwarding parameter or alias result has been materialized locally.
template <typename T>
using pr_rsv_named_lvalue_t = ::std::add_lvalue_reference_t<::std::remove_reference_t<T>>;

template <typename T>
using pr_rsv_alias_result_t = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief Computes the exception contract of the compile-time reserve-size query selected for one source category.
/// @details An actual alias expression is type-only: `io_print_alias` is never evaluated while computing a static
///          reserve extent. Its result is nevertheless modeled as the named lvalue used by runtime materialization.
///          A non-aliased source retains the historical named-parameter model, including immovable rvalues.
template <::std::integral char_type, typename T>
inline constexpr bool pr_rsv_size_impl_nothrow = []() constexpr {
	if constexpr (::fast_io::alias_printable<T>)
	{
		using alias_result = ::fast_io::details::pr_rsv_alias_result_t<T>;
		using alias_expression = ::fast_io::details::pr_rsv_named_lvalue_t<alias_result>;
		if constexpr (::fast_io::reserve_printable<char_type, alias_expression>)
		{
			return noexcept(print_reserve_size(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<alias_result>>));
		}
	}
	else
	{
		using source_expression = ::fast_io::details::pr_rsv_named_lvalue_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, source_expression>)
		{
			return noexcept(print_reserve_size(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		}
	}
	return false;
}();

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t pr_rsv_size_impl() noexcept(::fast_io::details::pr_rsv_size_impl_nothrow<char_type, T>)
{
	if constexpr (::fast_io::alias_printable<T>)
	{
		using alias_result = ::fast_io::details::pr_rsv_alias_result_t<T>;
		using alias_expression = ::fast_io::details::pr_rsv_named_lvalue_t<alias_result>;
		using alias_type = ::std::remove_cvref_t<alias_result>;
		constexpr bool error{::fast_io::reserve_printable<char_type, alias_expression>};
		if constexpr (error)
		{
			// Static reserve size depends only on the normalized result type. Keeping it in a local constant proves that
			// querying `pr_rsv_size` does not evaluate a potentially fallible runtime alias customization.
			constexpr ::std::size_t sz{
				print_reserve_size(::fast_io::io_reserve_type<char_type, alias_type>)};
			return sz;
		}
		else
		{
			static_assert(error, "type is not reserve_printable");
			return 0;
		}
	}
	else
	{
		using source_expression = ::fast_io::details::pr_rsv_named_lvalue_t<T>;
		using source_type = ::std::remove_cvref_t<T>;
		constexpr bool error{::fast_io::reserve_printable<char_type, source_expression>};
		if constexpr (error)
		{
			constexpr ::std::size_t sz{
				print_reserve_size(::fast_io::io_reserve_type<char_type, source_type>)};
			return sz;
		}
		else
		{
			static_assert(error, "type is not reserve_printable");
			return 0;
		}
	}
}

/// @brief Computes the exception contract of reserve materialization from the exact source expression.
/// @details Aliasing is probed and invoked with `T`, including its cv/ref category. The alias result is then named
///          locally, so the reserve CPO is checked with that lvalue expression while its tag uses only the alias's
///          identity type. This exactly mirrors the executable branch and prevents either overload resolution or a
///          potentially throwing customization from being hidden by a synthetic category.
template <typename Iter, typename T>
inline constexpr bool pr_rsv_to_iterator_unchecked_nothrow = []() constexpr {
	using char_type = ::std::iter_value_t<Iter>;
	if constexpr (::fast_io::alias_printable<T>)
	{
		using alias_result = ::fast_io::details::pr_rsv_alias_result_t<T>;
		using alias_expression = ::fast_io::details::pr_rsv_named_lvalue_t<alias_result>;
		using alias_type = ::std::remove_cvref_t<alias_result>;
		if constexpr (::fast_io::reserve_printable<char_type, alias_expression>)
		{
			return noexcept(::fast_io::io_print_alias(::std::declval<T>())) &&
				   noexcept(print_reserve_define(
					   ::fast_io::io_reserve_type<char_type, alias_type>, ::std::declval<Iter &>(),
					   ::std::declval<alias_expression>()));
		}
	}
	else
	{
		using source_expression = ::fast_io::details::pr_rsv_named_lvalue_t<T>;
		using source_type = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, source_expression>)
		{
			return noexcept(print_reserve_define(
				::fast_io::io_reserve_type<char_type, source_type>, ::std::declval<Iter &>(),
				::std::declval<source_expression>()));
		}
	}
	return false;
}();

#if defined(__HERBCEPTIONS__)
/// @brief Classifies the deterministic error effect of one unchecked reserve materialization transaction.
/// @details The queried expression matches the executed branch exactly. An actual alias is normalized through the
///          public CPO, named, and then consumed by the reserve writer; a non-aliased source remains the original named
///          forwarding parameter and therefore acquires no construction effect. The result is independent from
///          traditional `noexcept` because Herbceptions use a distinct ABI.
template <typename Iter, typename T>
inline constexpr bool pr_rsv_to_iterator_unchecked_herbceptions_may_fail = []() constexpr {
	using char_type = ::std::iter_value_t<Iter>;
	if constexpr (::fast_io::alias_printable<T>)
	{
		using alias_result = ::fast_io::details::pr_rsv_alias_result_t<T>;
		using alias_expression = ::fast_io::details::pr_rsv_named_lvalue_t<alias_result>;
		using alias_type = ::std::remove_cvref_t<alias_result>;
		if constexpr (::fast_io::reserve_printable<char_type, alias_expression>)
		{
			return throws((::fast_io::io_print_alias(::std::declval<T>()))) ||
				   throws((print_reserve_define(
					   ::fast_io::io_reserve_type<char_type, alias_type>, ::std::declval<Iter &>(),
					   ::std::declval<alias_expression>())));
		}
	}
	else
	{
		using source_expression = ::fast_io::details::pr_rsv_named_lvalue_t<T>;
		using source_type = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, source_expression>)
		{
			return throws((print_reserve_define(
				::fast_io::io_reserve_type<char_type, source_type>, ::std::declval<Iter &>(),
				::std::declval<source_expression>())));
		}
	}
	return false;
}();
#endif

} // namespace details

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t pr_rsv_size{::fast_io::details::pr_rsv_size_impl<char_type, T>()};

template <::std::random_access_iterator Iter, typename T>
inline constexpr Iter pr_rsv_to_iterator_unchecked(Iter it, T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::pr_rsv_to_iterator_unchecked_herbceptions_may_fail<Iter, T>),
		::fast_io::details::pr_rsv_to_iterator_unchecked_nothrow<Iter, T>)
{
	// The declared deterministic effect is the disjunction of alias normalization and reserve emission. Public alias
	// normalization remains confined to the real-alias branch, while the non-alias branch preserves the historical
	// named lvalue and therefore introduces neither a construction nor a phase-1 ownership change.
	using char_type = ::std::iter_value_t<Iter>;
	if constexpr (::fast_io::alias_printable<T>)
	{
		decltype(auto) alias_value = ::fast_io::io_print_alias(::std::forward<T>(t));
		using alias_expression = decltype((alias_value));
		using alias_type = ::std::remove_cvref_t<decltype(alias_value)>;
		constexpr bool error{::fast_io::reserve_printable<char_type, alias_expression>};
		if constexpr (error)
		{
			return print_reserve_define(
				::fast_io::io_reserve_type<char_type, alias_type>, it, alias_value);
		}
		else
		{
			static_assert(error, "type is not reserve_printable");
			return it;
		}
	}
	else
	{
		using source_expression = decltype((t));
		using source_type = ::std::remove_cvref_t<T>;
		constexpr bool error{::fast_io::reserve_printable<char_type, source_expression>};
		if constexpr (error)
		{
			return print_reserve_define(
				::fast_io::io_reserve_type<char_type, source_type>, it, t);
		}
		else
		{
			static_assert(error, "type is not reserve_printable");
			return it;
		}
	}
}

template <::std::integral char_type, ::std::size_t n, typename T>
inline constexpr char_type *pr_rsv_to_c_array(char_type (&buffer)[n], T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::pr_rsv_to_iterator_unchecked_herbceptions_may_fail<char_type *, T>),
		noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
			::std::declval<char_type *>(), ::std::declval<T>())))
{
	constexpr bool error{(::fast_io::pr_rsv_size<char_type, T>) <= n};
	if constexpr (error)
	{
		return pr_rsv_to_iterator_unchecked(buffer, ::std::forward<T>(t));
	}
	else
	{
		static_assert(error, "C array size is not enough");
		return buffer;
	}
}

// Each standard library uses a different <array> include guard. The former duplicate `_GLIBCXX_ARRAY` check made the
// overload disappear on libc++, even though `std::array` was already complete at this point.
#if defined(_GLIBCXX_ARRAY) || defined(_LIBCPP_ARRAY) || defined(_ARRAY_)
template <::std::integral char_type, ::std::size_t n, typename T>
inline constexpr typename ::std::array<char_type, n>::iterator
pr_rsv_to_array(::std::array<char_type, n> &buffer, T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::pr_rsv_to_iterator_unchecked_herbceptions_may_fail<char_type *, T>),
		noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
			::std::declval<char_type *>(), ::std::declval<T>())))
{
	constexpr bool error{(::fast_io::pr_rsv_size<char_type, T>) <= n};
	if constexpr (error)
	{
		return pr_rsv_to_iterator_unchecked(buffer.data(), ::std::forward<T>(t)) - buffer.data() + buffer.begin();
	}
	else
	{
		static_assert(error, "array size is not enough");
		return buffer.begin();
	}
}
#endif

namespace details
{

template <::std::integral char_type, typename T>
inline constexpr ::fast_io::parse_result<char_type const *> parse_by_scan_impl(char_type const *first,
															   char_type const *last, T &&t)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	// The tag is intentionally unqualified, but scanner overload resolution must see the normalized proxy's actual
	// cv-qualified lvalue. This is the same admission rule used by stream scanning and prevents a terminal-range bridge
	// from selecting a mutable-only CPO for a const reference alias.
	if constexpr (::fast_io::precise_reserve_scannable<char_type, T>)
	{
		constexpr ::std::size_t n{scan_precise_reserve_size(io_reserve_type<char_type, scanner_type>)};
		char_type const *next{first};
		if constexpr (n != 0u)
		{
			// Equality must be handled before subtraction because `{nullptr,nullptr}` is a valid empty view but does not
			// denote an array on which pointer subtraction is defined. Positive extents alone need a real input span.
			if (first == last) [[unlikely]]
			{
				return {first, ::fast_io::parse_code::end_of_file};
			}
			::std::size_t const diff{static_cast<::std::size_t>(last - first)};
			if (diff < n) [[unlikely]]
			{
				return {first, ::fast_io::parse_code::end_of_file};
			}
			next = first + n;
		}
		char_type dummy{};
		// The zero-extent protocol may initialize a target but may not inspect a character. Supplying a valid object
		// address for an empty null range keeps that contract usable under sanitizers without performing null arithmetic.
		char_type const *const scan_buffer{n == 0u && first == last ? __builtin_addressof(dummy) : first};
		if constexpr (precise_reserve_scannable_no_error<char_type, T>)
		{
			scan_precise_reserve_define(io_reserve_type<char_type, scanner_type>, scan_buffer, t);
			return {next, ::fast_io::parse_code::ok};
		}
		else
		{
			auto const code{scan_precise_reserve_define(io_reserve_type<char_type, scanner_type>, scan_buffer, t)};
			// A precise validator reports only a code, not an iterator. The protocol describes an exact input record, so
			// that extent is consumed once enough input exists even when validation fails. This matches stream staging across
			// refill boundaries, where generic input devices cannot rewind already-read characters.
			return {next, code};
		}
	}
	else if constexpr (::fast_io::contiguous_scannable<char_type, T>)
	{
		auto const result{scan_contiguous_define(io_reserve_type<char_type, scanner_type>, first, last, t)};
		// Contiguous and context scanners have the same closed input interval. Checking both prevents capability order
		// from deciding whether an escaped CPO iterator is accepted by this terminal-range API.
		if (!::fast_io::details::scan_iterator_in_current_chunk(first, last, result.iter)) [[unlikely]]
		{
			return {first, ::fast_io::parse_code::invalid};
		}
		return result;
	}
	else if constexpr (::fast_io::context_scannable<char_type, T>)
	{
		using state_type = ::fast_io::details::scan_context_state_t<char_type, scanner_type>;
		return ::fast_io::details::with_scan_context_state<state_type>([&](state_type &state) {
			auto current{first};
			for (;;)
			{
				if (current == last)
				{
					// A terminal range has no future refill. EOF, rather than an empty context call, owns the
					// completion of an exhausted partial token. This is the same state transition used by the
					// ordinary ibuffer dispatcher and lets a scanner distinguish empty input from a completed
					// token whose delimiter is optional at EOF.
					auto const ec{
						scan_context_eof_define(io_reserve_type<char_type, scanner_type>, state, t)};
					// There is no later fragment in a terminal range. Preserving `partial` would advertise progress that this
					// API can never make and disagrees with the refillable dispatcher, which rejects the same EOF transition.
					return ::fast_io::parse_result<char_type const *>{
						current, ec == parse_code::partial ? parse_code::invalid : ec};
				}
				auto [it, ec] = scan_context_define(
					io_reserve_type<char_type, scanner_type>, state, current, last, t);
				// A context CPO must return an iterator in the range it was given. Rejecting both backward and
				// past-the-end iterators before observing the code prevents a malformed customization from escaping
				// this range on success or from creating a reverse/out-of-bounds partial loop.
				if (!::fast_io::details::scan_iterator_in_current_chunk(current, last, it)) [[unlikely]]
				{
					return ::fast_io::parse_result<char_type const *>{current, parse_code::invalid};
				}
				if (ec != parse_code::partial)
				{
					return ::fast_io::parse_result<char_type const *>{it, ec};
				}
				// `partial` describes protocol state, not buffer exhaustion. A scanner may consume only a
				// prefix while changing phase, so the unconsumed suffix must be offered to the same state
				// before EOF. Conversely, returning partial without consuming an available character cannot
				// become productive without a refill and would make this terminal dispatcher spin forever.
				if (it == current) [[unlikely]]
				{
					return ::fast_io::parse_result<char_type const *>{current, parse_code::invalid};
				}
				current = it;
			}
		});
	}
	else
	{
		constexpr bool not_scannable{context_scannable<char_type, T>};
		static_assert(not_scannable, "type not scannable. need context_scannable");
		return false;
	}
}

} // namespace details

inline namespace io
{

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(nodiscard)
[[nodiscard("NEVER discard return pointer and parse code from parse_by_scan")]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *> parse_by_scan(char_type const *first, char_type const *last,
																					  T &&t)
{
	using mytype = decltype(io_scan_forward<char_type>(io_scan_alias(t)));
	constexpr bool allscannable{::fast_io::precise_reserve_scannable<char_type, mytype> ||
								::fast_io::contiguous_scannable<char_type, mytype> ||
								::fast_io::context_scannable<char_type, mytype>};
	if constexpr (allscannable)
	{
		return ::fast_io::details::parse_by_scan_impl(first, last, io_scan_forward<char_type>(io_scan_alias(t)));
	}
	else
	{
		static_assert(allscannable, "type not scannable. need context_scannable");
	}
}

} // namespace io

} // namespace fast_io

#pragma once

namespace fast_io
{

namespace details
{

template <typename char_type, typename T>
inline constexpr bool lc_condition_scatter_branch =
	::fast_io::lc_scatter_printable<char_type, T> || ::fast_io::scatter_printable<char_type, T>;

/// @brief Admits a zero-copy locale condition only when both possible branches expose a scatter.
/// @details Strategy selection occurs before the run-time predicate is known. Requiring both branches proves that the
///          return representation is valid for either value, while requiring at least one locale scatter prevents this
///          overload from competing with the ordinary condition protocol for a purely non-locale node.
template <typename char_type, typename T1, typename T2>
concept lc_condition_scatter_printable =
	::std::integral<char_type> &&
	lc_condition_scatter_branch<char_type, T1> && lc_condition_scatter_branch<char_type, T2> &&
	(::fast_io::lc_scatter_printable<char_type, T1> ||
	 ::fast_io::lc_scatter_printable<char_type, T2>);

/// @brief Admits contiguous condition materialization when both branches have a proven locale-or-ordinary protocol.
/// @details A condition may select an ordinary fallback on one side and a locale formatter on the other. Both sides
///          must still be measurable before the common dynamic-reserve protocol is advertised. The locale-presence
///          clause keeps ordinary conditions owned by the ordinary semantic engine.
template <typename char_type, typename T1, typename T2>
concept lc_condition_contiguous_printable =
	::std::integral<char_type> &&
	::fast_io::details::decay::lc_contiguous_printable<char_type, T1> &&
	::fast_io::details::decay::lc_contiguous_printable<char_type, T2> &&
	(::fast_io::lc_scatter_printable<char_type, T1> ||
	 ::fast_io::lc_dynamic_reserve_printable<char_type, T1> ||
	 ::fast_io::lc_scatter_printable<char_type, T2> ||
	 ::fast_io::lc_dynamic_reserve_printable<char_type, T2>);

/// @brief Proves the lifetime and repeatability of the scatter branch actually selected by the locale condition CPO.
/// @details Locale scatter has priority over ordinary scatter in `lc_condition_scatter`; an ordinary marker therefore
///          cannot repair an unmarked locale customization on the same type. This expression deliberately mirrors that
///          priority so marker propagation cannot certify a different protocol from the one the adapter executes.
template <typename char_type, typename T>
inline constexpr bool lc_condition_retained_scatter_branch = []() constexpr {
	if constexpr (::fast_io::lc_scatter_printable<char_type, T>)
	{
		return ::fast_io::lc_borrowed_scatter_source<char_type, T>;
	}
	else
	{
		return ::fast_io::borrowed_scatter_source<char_type, T>;
	}
}();

template <::std::integral char_type, typename T>
inline constexpr basic_io_scatter_t<char_type> lc_condition_scatter(
	basic_lc_all<char_type> const *all, T &value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::lc_scatter_printable<char_type, T &>)
	{
		return print_scatter_define(all, value);
	}
	else
	{
		return print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, value);
	}
}

} // namespace details

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_scatter_printable<char_type, T1 &, T2 &>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	basic_lc_all<char_type> const *all, ::fast_io::manipulators::condition<T1, T2> &condition)
{
	// The condition object belongs to the enclosing semantic frame. Borrow it so a scatter into an owning selected
	// branch retains that branch's lifetime instead of pointing into a destroyed adapter-local copy.
	if (condition.pred)
	{
		return ::fast_io::details::lc_condition_scatter<char_type>(all, condition.t1);
	}
	return ::fast_io::details::lc_condition_scatter<char_type>(all, condition.t2);
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_scatter_printable<char_type, T1 &, T2 &> &&
			 ::fast_io::details::lc_condition_retained_scatter_branch<char_type, T1 &> &&
			 ::fast_io::details::lc_condition_retained_scatter_branch<char_type, T2 &>
inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<
		char_type, ::fast_io::manipulators::condition<T1, T2>>) noexcept
{
	// Selection observes exactly one arm, but the marker is type-level and the run-time predicate is unknown to a
	// retained planner. Requiring both alternatives proves the descriptor contract for every value of the condition.
	return {};
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_scatter_printable<
		char_type,
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t1)),
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t2))>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	basic_lc_all<char_type> const *all,
	::fast_io::manipulators::condition<T1, T2> const &condition)
{
	if (condition.pred)
	{
		return ::fast_io::details::lc_condition_scatter<char_type>(all, condition.t1);
	}
	return ::fast_io::details::lc_condition_scatter<char_type>(all, condition.t2);
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_contiguous_printable<char_type, T1 &, T2 &>
inline constexpr ::std::size_t print_reserve_size(
	basic_lc_all<char_type> const *all, ::fast_io::manipulators::condition<T1, T2> &condition)
{
	if (condition.pred)
	{
		return ::fast_io::details::decay::lc_contiguous_size<char_type>(all, condition.t1);
	}
	return ::fast_io::details::decay::lc_contiguous_size<char_type>(all, condition.t2);
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_contiguous_printable<
		char_type,
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t1)),
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t2))>
inline constexpr ::std::size_t print_reserve_size(
	basic_lc_all<char_type> const *all,
	::fast_io::manipulators::condition<T1, T2> const &condition)
{
	if (condition.pred)
	{
		return ::fast_io::details::decay::lc_contiguous_size<char_type>(all, condition.t1);
	}
	return ::fast_io::details::decay::lc_contiguous_size<char_type>(all, condition.t2);
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_contiguous_printable<char_type, T1 &, T2 &>
inline constexpr char_type *print_reserve_define(
	basic_lc_all<char_type> const *all, char_type *destination,
	::fast_io::manipulators::condition<T1, T2> &condition)
{
	if (condition.pred)
	{
		return ::fast_io::details::decay::lc_contiguous_define<char_type>(
			all, destination, condition.t1);
	}
	return ::fast_io::details::decay::lc_contiguous_define<char_type>(
		all, destination, condition.t2);
}

template <::std::integral char_type, typename T1, typename T2>
	requires ::fast_io::details::lc_condition_contiguous_printable<
		char_type,
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t1)),
		decltype((::std::declval<::fast_io::manipulators::condition<T1, T2> const &>().t2))>
inline constexpr char_type *print_reserve_define(
	basic_lc_all<char_type> const *all, char_type *destination,
	::fast_io::manipulators::condition<T1, T2> const &condition)
{
	if (condition.pred)
	{
		return ::fast_io::details::decay::lc_contiguous_define<char_type>(
			all, destination, condition.t1);
	}
	return ::fast_io::details::decay::lc_contiguous_define<char_type>(
		all, destination, condition.t2);
}

} // namespace fast_io

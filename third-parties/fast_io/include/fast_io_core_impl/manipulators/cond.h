#pragma once

/*
 * Conditional semantic record node (`cond`) for the CPO/semantic level.
 *
 * This node retains two alternative IO object graphs and a run-time selector;
 * exactly one branch contributes to the active print/concat record. Branches
 * are alias-normalized and stored with the same ownership rules as pack
 * elements. Capacity and strategy planning must inspect only the selected
 * branch, while neither branch is allowed to perform IO during construction.
 */

#include "forward.h"

namespace fast_io
{

namespace details
{

template <typename T1>
concept cond_value_transferable =
	::fast_io::details::io_print_forward_transport_by_value<T1>;

// Condition storage is an entry-decay decision, not an independent calling-convention model. Reusing the common
// transport policy keeps Microsoft x64's exact 1/2/4/8-byte rule, SysV eightbyte envelopes, AAPCS, RISC-V, LoongArch,
// PowerPC, MIPS, SPARC, s390, capability, and conservative unknown-ABI policies identical at both boundaries. The old
// `Windows ? 8 : 2 * sizeof(size_t)` shortcut admitted indirect or stack-passed aggregates on several ABIs and also
// disagreed with print forwarding about non-trivial-for-calls classes. Incomplete lvalues remain safe: the shared
// admission checks completeness before every standard trait and selects reference storage when the conservative
// by-value transport envelope does not admit the object.

template <typename T>
using cond_alias_result = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief Selects branch storage without copying a borrowed alias proxy or borrowing from a temporary.
/// @details The proof is identical to pack storage: only an alias reference obtained from an lvalue source may remain
///          a reference.  Every rvalue-derived result is materialized, so a condition object never retains a pointer
///          into an already-destroyed source temporary.  For an ordinary non-aliased lvalue, small trivial values are
///          copied as a calling-convention optimization and all other values retain their actual cv-qualified
///          reference identity.
template <typename T>
using cond_alias_type = ::std::conditional_t<
	::std::is_lvalue_reference_v<T &&> && ::fast_io::alias_printable<T> && ::std::is_lvalue_reference_v<::fast_io::details::cond_alias_result<T>>,
	::fast_io::details::cond_alias_result<T>,
	::std::conditional_t<
		::std::is_lvalue_reference_v<T &&> && !::fast_io::alias_printable<T>,
		::std::conditional_t<::fast_io::details::cond_value_transferable<T>, ::std::remove_cvref_t<T>, T>,
		::std::remove_cvref_t<::fast_io::details::cond_alias_result<T>>>>;

/// @brief Admits exactly the explicit alias-to-storage conversion executed by `cond_store`.
/// @details The requires-expression turns an ill-formed conversion into constraint-false. The independent move test
///          models the subsequent by-value semantic boundary; references need no such proof because they preserve the
///          existing object's identity instead of creating another owned transport object.
template <typename T>
concept cond_alias_storable =
	requires {
		static_cast<::fast_io::details::cond_alias_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>()));
	} &&
	(::std::is_lvalue_reference_v<::fast_io::details::cond_alias_type<T>> ||
	 ::std::constructible_from<
		 ::std::remove_cvref_t<::fast_io::details::cond_alias_type<T>>,
		 ::std::remove_cvref_t<::fast_io::details::cond_alias_type<T>> &&>);

/// @brief Computes the exception contract of the exact condition-arm storage expression.
/// @details The final condition enters print/concat's by-value decay pipeline. An owned result must therefore remain
///          movable after its initial elided construction; accepting an immovable prvalue would merely defer a hard
///          error to that decay boundary and create a compiler-specific semantic type. Stable lvalue proxies remain
///          references and need not be movable. An rvalue alias exposing a borrowed noncopyable subobject is rejected
///          independently by the exact storage expression above.
template <typename T>
inline constexpr bool cond_alias_nothrow_constructible = []() constexpr {
	if constexpr (::fast_io::details::cond_alias_storable<T>)
	{
		return noexcept(static_cast<::fast_io::details::cond_alias_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>())));
	}
	else
	{
		return false;
	}
}();

/// @brief Performs the one alias evaluation and the explicit conversion proved by condition admission.
/// @details Aggregate element initialization uses copy-initialization semantics and therefore can reject an explicit
///          converting constructor even though the admission probe's `static_cast` accepted it. Routing every factory
///          arm through this helper makes the constraint, exception proof, and executed conversion identical. It also
///          preserves the entry policy: alias once, then store either the compact owned value or the exact reference.
template <typename T>
	requires ::fast_io::details::cond_alias_storable<T>
inline constexpr ::fast_io::details::cond_alias_type<T> cond_store(T &&t)
	noexcept(::fast_io::details::cond_alias_nothrow_constructible<T>)
{
	return static_cast<::fast_io::details::cond_alias_type<T>>(
		::fast_io::io_print_alias(::std::forward<T>(t)));
}

} // namespace details

namespace manipulators
{

/// @brief Identifies a semantic node that selects one of two printable arms at run time.
/// @details The tag is consumed by print/concat planning and has no printable representation itself.
struct condition_manip_tag_t
{};

/// @brief Stores two normalized printable alternatives and the predicate selecting the first arm.
/// @details Both arms are constructed when the manipulator is created; only the selected arm is formatted. The factory
///          may reverse physical member order to reduce padding while inversely adjusting `pred`, preserving semantics.
template <typename T1, typename T2>
struct condition
{
	using manip_tag = manip_tag_t;
	using semantic_tag = condition_manip_tag_t;
	using alias_type1 = T1;
	using alias_type2 = T2;
	bool pred;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	alias_type1 t1;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	alias_type2 t2;
};

/// @brief Selects exactly one of two printable alternatives at formatting time.
/// @details `pred == true` emits `t1`, otherwise `t2`. Both arguments are normalized and stored immediately, so their
///          construction side effects occur regardless of which arm is later emitted. No separator is added.
template <typename T1, typename T2>
	requires(::fast_io::details::cond_alias_storable<T1> &&
			 ::fast_io::details::cond_alias_storable<T2>)
inline constexpr auto cond(bool pred, T1 &&t1, T2 &&t2) noexcept(::fast_io::details::cond_alias_nothrow_constructible<T1> &&
																 ::fast_io::details::cond_alias_nothrow_constructible<T2>)
{
	using t1aliastype = ::fast_io::details::cond_alias_type<T1>;
	using t2aliastype = ::fast_io::details::cond_alias_type<T2>;
	// Initialize final aggregate members from `cond_store`'s one alias evaluation and explicit conversion. This keeps
	// lifetime flow visible to the optimizer and introduces no additional semantic transport type after normalization.

	if constexpr (::std::same_as<t1aliastype, ::fast_io::io_null_t> &&
				  ::std::same_as<t2aliastype, ::fast_io::io_null_t>)
	{
		return ::fast_io::io_null;
	}
	else if constexpr (
		::std::same_as<t1aliastype, t2aliastype> &&
		!::std::is_reference_v<t1aliastype> &&
		::std::is_trivially_copyable_v<t1aliastype> &&
		::std::is_nothrow_move_constructible_v<t1aliastype>)
	{
		// The two normalized arms have one representation and no observable destruction. Construct both first so alias
		// evaluation keeps the semantic node's ordering/effect rule, then return only the selected value. This collapses
		// the common homogeneous condition before print planning without changing either argument's normalization.
		t1aliastype first{
			::fast_io::details::cond_store(::std::forward<T1>(t1))};
		t2aliastype second{
			::fast_io::details::cond_store(::std::forward<T2>(t2))};
		return pred ? ::std::move(first) : ::std::move(second);
	}
	else if constexpr (
		sizeof(condition<t2aliastype, t1aliastype>) <
		sizeof(condition<t1aliastype, t2aliastype>))
	{
		// Compare the two complete aggregate layouts rather than guessing from the members' sizes. `sizeof(T1) <
		// sizeof(T2)` is not a padding proof: a smaller but more strongly aligned arm can make largest-first ordering larger.
		// Inverting the predicate proves that physical order is unobservable to formatting semantics. Equal-size layouts
		// preserve source order, so aliases are reordered only when the object representation has measured evidence.
		return condition<t2aliastype, t1aliastype>{!pred,
												   ::fast_io::details::cond_store(::std::forward<T2>(t2)),
												   ::fast_io::details::cond_store(::std::forward<T1>(t1))};
	}
	else
	{
		return condition<t1aliastype, t2aliastype>{pred,
												   ::fast_io::details::cond_store(::std::forward<T1>(t1)),
												   ::fast_io::details::cond_store(::std::forward<T2>(t2))};
	}
}

/// @brief Conditionally emits one printable value or nothing.
/// @details `pred == true` emits `t1`; `false` selects `io_null`. The argument is still normalized and stored when the
///          manipulator is created.
template <typename T1>
	requires ::fast_io::details::cond_alias_storable<T1>
inline constexpr auto cond(bool pred, T1 &&t1) noexcept(::fast_io::details::cond_alias_nothrow_constructible<T1>)
{
	using t1aliastype = ::fast_io::details::cond_alias_type<T1>;
	// As in the two-arm overload, the normalized storage expression directly constructs the semantic member.

	constexpr bool type_match{::std::same_as<t1aliastype, ::fast_io::io_null_t>};
	if constexpr (type_match)
	{
		return ::fast_io::io_null;
	}
	else
	{
		return condition<t1aliastype, ::fast_io::io_null_t>{
			pred,
			::fast_io::details::cond_store(::std::forward<T1>(t1)),
			::fast_io::io_null};
	}
}

} // namespace manipulators

/// @brief Propagates read-prefetch provenance only when both possible condition arms are safe.
/// @details The run-time predicate is not part of the type, so a type-level policy cannot justify itself from the arm
///          selected by one particular value. Requiring both arms makes the promise valid for every instance;
///          `io_null_t` is admitted only as the vacuous alternative which exposes no external range.
template <typename T1, typename T2>
	requires(::fast_io::prfch_cacheable_read_or_no_external_range<T1> &&
			 ::fast_io::prfch_cacheable_read_or_no_external_range<T2>)
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::condition<T1, T2>>) noexcept
{
	return {};
}

#if 0
	namespace details
	{

template <typename T1, typename T2>
concept cond_transferable_value = ::std::is_trivially_copyable_v<::fast_io::manipulators::condition<T1, T2>> &&
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
								  sizeof(::fast_io::manipulators::condition<T1, T2>) <= 8u
#else
								  sizeof(::fast_io::manipulators::condition<T1, T2>) <= (sizeof(::std::size_t) * 2)
#endif
	;

template <::std::integral char_type, typename T1, typename T2>
struct cond_print_context
{
	using context_type1 =
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, T1>))>::type;
	using context_type2 =
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, T2>))>::type;
	context_type1 state1;
	context_type2 state2;

	inline constexpr context_print_result<char_type *>
	print_context_define(::fast_io::manipulators::condition<T1, T2> c, char_type *begin, char_type *end)
	{
		if (c.pred)
		{
			return state1.print_context_define(c.t1, begin, end);
		}
		else
		{
			return state2.print_context_define(c.t2, begin, end);
		}
	}
};

template <::std::integral char_type, typename T1>
struct cond_print_context_first
{
	using context_type1 =
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, T1>))>::type;
	context_type1 state1;

	inline constexpr context_print_result<char_type *>
	print_context_define(::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> c, char_type *begin,
						 char_type *end)
	{
		if (c.pred)
		{
			return state1.print_context_define(c.t1, begin, end);
		}
		else
		{
			return {begin, true};
		}
	}
};

template <::std::integral char_type, typename T2>
struct cond_print_context_second
{
	using context_type2 =
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, T2>))>::type;
	context_type2 state2;

	inline constexpr context_print_result<char_type *>
	print_context_define(::fast_io::manipulators::condition<::fast_io::io_null_t, T2> c, char_type *begin,
						 char_type *end)
	{
		if (!c.pred)
		{
			return state2.print_context_define(c.t2, begin, end);
		}
		else
		{
			return {begin, true};
		}
	}
};
} // namespace details

template <::std::integral char_type, typename T1, typename T2>
	requires(context_printable<char_type, T1> && context_printable<char_type, T2>)
inline constexpr auto
print_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>) noexcept
{
	return io_type_t<::fast_io::details::cond_print_context<char_type, T1, T2>>{};
}

template <::std::integral char_type, typename T1>
	requires(context_printable<char_type, T1>)
inline constexpr auto
print_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>) noexcept
{
	return io_type_t<::fast_io::details::cond_print_context_first<char_type, T1>>{};
}

template <::std::integral char_type, typename T2>
	requires(context_printable<char_type, T2>)
inline constexpr auto
print_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>) noexcept
{
	return io_type_t<::fast_io::details::cond_print_context_second<char_type, T2>>{};
}

template <::std::integral char_type, typename T1, typename T2>
	requires(context_printable_with_static_buffer_size<char_type, T1> &&
			 context_printable_with_static_buffer_size<char_type, T2>)
inline constexpr ::std::size_t
print_context_static_buffer_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>) noexcept
{
	constexpr ::std::size_t s1{print_context_static_buffer_size(io_reserve_type<char_type, T1>)};
	constexpr ::std::size_t s2{print_context_static_buffer_size(io_reserve_type<char_type, T2>)};
	if constexpr (s1 < s2)
	{
		return s2;
	}
	else
	{
		return s1;
	}
}

template <::std::integral char_type, typename T1>
	requires(context_printable_with_static_buffer_size<char_type, T1>)
inline constexpr ::std::size_t print_context_static_buffer_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>) noexcept
{
	return print_context_static_buffer_size(io_reserve_type<char_type, T1>);
}

template <::std::integral char_type, typename T2>
	requires(context_printable_with_static_buffer_size<char_type, T2>)
inline constexpr ::std::size_t print_context_static_buffer_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>) noexcept
{
	return print_context_static_buffer_size(io_reserve_type<char_type, T2>);
}

template <::std::integral char_type, typename T1, typename T2>
	requires(reserve_printable<char_type, T1> && reserve_printable<char_type, T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>) noexcept
{
	constexpr ::std::size_t s1{print_reserve_size(io_reserve_type<char_type, T1>)};
	constexpr ::std::size_t s2{print_reserve_size(io_reserve_type<char_type, T2>)};
	if constexpr (s1 < s2)
	{
		return s2;
	}
	else
	{
		return s1;
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires(scatter_printable<char_type, T1> && scatter_printable<char_type, T2> &&
			 details::cond_transferable_value<T1, T2>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>,
					 ::fast_io::manipulators::condition<T1, T2> c)
{
	if (c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T1>, c.t1)};
	}
	else
	{
		return {print_scatter_define(io_reserve_type<char_type, T2>, c.t2)};
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires(scatter_printable<char_type, T1> && scatter_printable<char_type, T2> &&
			 !details::cond_transferable_value<T1, T2>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>,
					 ::fast_io::manipulators::condition<T1, T2> const &c)
{
	if (c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T1>, c.t1)};
	}
	else
	{
		return {print_scatter_define(io_reserve_type<char_type, T2>, c.t2)};
	}
}

template <::std::integral char_type, typename T1>
	requires(reserve_printable<char_type, T1>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>) noexcept
{
	constexpr ::std::size_t s1{print_reserve_size(io_reserve_type<char_type, T1>)};
	return s1;
}

template <::std::integral char_type, typename T1>
	requires(scatter_printable<char_type, T1> && details::cond_value_transferable<T1>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>,
					 ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> c)
{
	if (c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T1>, c.t1)};
	}
	else
	{
		return basic_io_scatter_t<char_type>{};
	}
}

template <::std::integral char_type, typename T1>
	requires(scatter_printable<char_type, T1> && !details::cond_value_transferable<T1>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>,
					 ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> const &c)
{
	if (c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T1>, c.t1)};
	}
	else
	{
		return basic_io_scatter_t<char_type>{};
	}
}

template <::std::integral char_type, typename T2>
	requires(reserve_printable<char_type, T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>) noexcept
{
	constexpr ::std::size_t s2{print_reserve_size(io_reserve_type<char_type, T2>)};
	return s2;
}

template <::std::integral char_type, typename T2>
	requires(scatter_printable<char_type, T2> && details::cond_value_transferable<T2>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>,
					 ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> c)
{
	if (!c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T2>, c.t2)};
	}
	else
	{
		return basic_io_scatter_t<char_type>{};
	}
}

template <::std::integral char_type, typename T2>
	requires(scatter_printable<char_type, T2> && !details::cond_value_transferable<T2>)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>,
					 ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> const &c)
{
	if (!c.pred)
	{
		return {print_scatter_define(io_reserve_type<char_type, T2>, c.t2)};
	}
	else
	{
		return basic_io_scatter_t<char_type>{};
	}
}

namespace details
{

template <typename char_type, typename T1>
concept cond_ok_dynamic_rsv_printable_impl =
	reserve_printable<char_type, T1> || dynamic_reserve_printable<char_type, T1> || scatter_printable<char_type, T1>;

template <typename char_type, typename T1>
concept cond_ok_static_stack_size_impl =
	reserve_printable<char_type, ::std::remove_cvref_t<T1>> ||
	dynamic_reserve_with_possible_static_stack_size<char_type, ::std::remove_cvref_t<T1>>;

template <typename char_type, typename T1>
concept cond_ok_printable_impl = cond_ok_dynamic_rsv_printable_impl<char_type, T1> || printable<char_type, T1>;

template <::std::integral char_type, typename T1>
	requires(cond_ok_static_stack_size_impl<char_type, T1>)
inline constexpr ::std::size_t cond_print_reserve_static_stack_size_impl() noexcept
{
	using value_type = ::std::remove_cvref_t<T1>;
	if constexpr (reserve_printable<char_type, value_type>)
	{
		return print_reserve_size(io_reserve_type<char_type, value_type>);
	}
	else
	{
		return print_reserve_static_stack_size(io_reserve_type<char_type, value_type>);
	}
}

template <::std::integral char_type, typename T1>
	requires(cond_value_transferable<T1>)
inline constexpr ::std::size_t cond_print_reserve_size_impl(T1 t1)
{
	if constexpr (scatter_printable<char_type, T1>)
	{
		return print_scatter_define(io_reserve_type<char_type, T1>, t1).len;
	}
	else if constexpr (reserve_printable<char_type, T1>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, T1>)};
		return sz;
	}
	else
	{
		return print_reserve_size(io_reserve_type<char_type, T1>, t1);
	}
}

template <::std::integral char_type, typename T1>
	requires(!cond_value_transferable<T1>)
inline constexpr ::std::size_t cond_print_reserve_size_impl(T1 const &t1)
{
	if constexpr (scatter_printable<char_type, T1>)
	{
		return print_scatter_define(io_reserve_type<char_type, T1>, t1).len;
	}
	else if constexpr (reserve_printable<char_type, T1>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, T1>)};
		return sz;
	}
	else
	{
		return print_reserve_size(io_reserve_type<char_type, T1>, t1);
	}
}

template <::std::integral char_type, typename T1>
	requires(cond_value_transferable<T1>)
inline constexpr char_type *cond_print_reserve_define_impl(char_type *iter, T1 t1)
{
	if constexpr (scatter_printable<char_type, T1>)
	{
		return copy_scatter(print_scatter_define(io_reserve_type<char_type, T1>, t1), iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, T1>, iter, t1);
	}
}

template <::std::integral char_type, typename T1>
	requires(!cond_value_transferable<T1>)
inline constexpr char_type *cond_print_reserve_define_impl(char_type *iter, T1 const &t1)
{
	if constexpr (scatter_printable<char_type, T1>)
	{
		return copy_scatter(print_scatter_define(io_reserve_type<char_type, T1>, t1), iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, T1>, iter, t1);
	}
}

} // namespace details

template <::std::integral char_type, typename T1, typename T2>
	requires((details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			  details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>) &&
			 (!(scatter_printable<char_type, T1> && scatter_printable<char_type, T2>)) &&
			 details::cond_transferable_value<T1, T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>,
				   ::fast_io::manipulators::condition<T1, T2> c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T1>(c.t1);
	}
	else
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T2>(c.t2);
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires((details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			  details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>) &&
			 (!(scatter_printable<char_type, T1> && scatter_printable<char_type, T2>)) &&
			 !details::cond_transferable_value<T1, T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>,
				   ::fast_io::manipulators::condition<T1, T2> const &c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T1>(c.t1);
	}
	else
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T2>(c.t2);
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires(details::cond_ok_static_stack_size_impl<char_type, T1> &&
			 details::cond_ok_static_stack_size_impl<char_type, T2>)
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>) noexcept
{
	constexpr ::std::size_t s1{
		::fast_io::details::cond_print_reserve_static_stack_size_impl<char_type, T1>()};
	constexpr ::std::size_t s2{
		::fast_io::details::cond_print_reserve_static_stack_size_impl<char_type, T2>()};
	if constexpr (s1 < s2)
	{
		return s2;
	}
	else
	{
		return s1;
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires((details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			  details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>) &&
			 (!(scatter_printable<char_type, T1> && scatter_printable<char_type, T2>)) &&
			 details::cond_transferable_value<T1, T2>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>, char_type *iter,
					 ::fast_io::manipulators::condition<T1, T2> c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T1>(iter, c.t1);
	}
	else
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T2>(iter, c.t2);
	}
}

template <::std::integral char_type, typename T1, typename T2>
	requires((details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			  details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>) &&
			 (!(scatter_printable<char_type, T1> && scatter_printable<char_type, T2>)) &&
			 !details::cond_transferable_value<T1, T2>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>, char_type *iter,
					 ::fast_io::manipulators::condition<T1, T2> const &c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T1>(iter, c.t1);
	}
	else
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T2>(iter, c.t2);
	}
}

template <::std::integral char_type, typename T1>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> && !scatter_printable<char_type, T1> &&
			 details::cond_value_transferable<T1>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>,
				   ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T1>(c.t1);
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T1>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> && !scatter_printable<char_type, T1> &&
			 !details::cond_value_transferable<T1>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>,
				   ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> const &c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T1>(c.t1);
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T1>
	requires(details::cond_ok_static_stack_size_impl<char_type, T1>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>) noexcept
{
	return ::fast_io::details::cond_print_reserve_static_stack_size_impl<char_type, T1>();
}

template <::std::integral char_type, typename T1>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> && !scatter_printable<char_type, T1> &&
			 details::cond_value_transferable<T1>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>, char_type *iter,
					 ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T1>(iter, c.t1);
	}
	else
	{
		return iter;
	}
}

template <::std::integral char_type, typename T1>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> && !scatter_printable<char_type, T1> &&
			 !details::cond_value_transferable<T1>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>, char_type *iter,
					 ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> const &c)
{
	if (c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T1>(iter, c.t1);
	}
	else
	{
		return iter;
	}
}

template <::std::integral char_type, typename T2>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> && !scatter_printable<char_type, T2> &&
			 details::cond_value_transferable<T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>,
				   ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> c)
{
	if (!c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T2>(c.t2);
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T2>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> && !scatter_printable<char_type, T2> &&
			 !details::cond_value_transferable<T2>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>,
				   ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> const &c)
{
	if (!c.pred)
	{
		return ::fast_io::details::cond_print_reserve_size_impl<char_type, T2>(c.t2);
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T2>
	requires(details::cond_ok_static_stack_size_impl<char_type, T2>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>) noexcept
{
	return ::fast_io::details::cond_print_reserve_static_stack_size_impl<char_type, T2>();
}

template <::std::integral char_type, typename T2>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> && !scatter_printable<char_type, T2> &&
			 details::cond_value_transferable<T2>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>, char_type *iter,
					 ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> c)
{
	if (!c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T2>(iter, c.t2);
	}
	else
	{
		return iter;
	}
}

template <::std::integral char_type, typename T2>
	requires(details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> && !scatter_printable<char_type, T2> &&
			 !details::cond_value_transferable<T2>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>, char_type *iter,
					 ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> const &c)
{
	if (!c.pred)
	{
		return ::fast_io::details::cond_print_reserve_define_impl<char_type, T2>(iter, c.t2);
	}
	else
	{
		return iter;
	}
}

template <::std::integral char_type, typename T1, typename T2, typename bop>
	requires((details::cond_ok_printable_impl<char_type, T1> && details::cond_ok_printable_impl<char_type, T2>) &&
			 (!(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
				details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>)) &&
			 details::cond_transferable_value<T1, T2>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>, bop b,
								   ::fast_io::manipulators::condition<T1, T2> c)
{
	if (c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t1);
	}
	else
	{
		::fast_io::operations::print_freestanding<false>(b, c.t2);
	}
}

template <::std::integral char_type, typename T1, typename T2, typename bop>
	requires((details::cond_ok_printable_impl<char_type, T1> && details::cond_ok_printable_impl<char_type, T2>) &&
			 (!(details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
				details::cond_ok_dynamic_rsv_printable_impl<char_type, T2>)) &&
			 !details::cond_transferable_value<T1, T2>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, T2>>, bop b,
								   ::fast_io::manipulators::condition<T1, T2> const &c)
{
	if (c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t1);
	}
	else
	{
		::fast_io::operations::print_freestanding<false>(b, c.t2);
	}
}

template <::std::integral char_type, typename T1, typename bop>
	requires(details::cond_ok_printable_impl<char_type, T1> && !details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			 details::cond_value_transferable<T1>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>, bop b,
								   ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> c)
{
	if (c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t1);
	}
}

template <::std::integral char_type, typename T1, typename bop>
	requires(details::cond_ok_printable_impl<char_type, T1> && !details::cond_ok_dynamic_rsv_printable_impl<char_type, T1> &&
			 !details::cond_value_transferable<T1>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t>>, bop b,
								   ::fast_io::manipulators::condition<T1, ::fast_io::io_null_t> const &c)
{
	if (c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t1);
	}
}

template <::std::integral char_type, typename T2, typename bop>
	requires(details::cond_ok_printable_impl<char_type, T2> && !details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> &&
			 details::cond_value_transferable<T2>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>, bop b,
								   ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> c)
{
	if (!c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t2);
	}
}

template <::std::integral char_type, typename T2, typename bop>
	requires(details::cond_ok_printable_impl<char_type, T2> && !details::cond_ok_dynamic_rsv_printable_impl<char_type, T2> &&
			 !details::cond_value_transferable<T2>)
inline constexpr void print_define(io_reserve_type_t<char_type, ::fast_io::manipulators::condition<::fast_io::io_null_t, T2>>, bop b,
								   ::fast_io::manipulators::condition<::fast_io::io_null_t, T2> const &c)
{
	if (!c.pred)
	{
		::fast_io::operations::print_freestanding<false>(b, c.t2);
	}
	}
#endif
} // namespace fast_io

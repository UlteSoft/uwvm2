#pragma once

/*
 * Structural classification of semantic print objects.
 *
 * The IO planner uses these traits to distinguish pack expansion, conditional
 * selection, and width nodes from ordinary printable leaves. Classification is
 * deliberately separate from leaf printability: semantic nodes are normalized
 * recursively until an active record of concrete leaf objects is obtained.
 */

#include "forward.h"
#include "static_arg.h"

namespace fast_io
{

namespace details
{

template <typename T>
inline constexpr bool is_print_pack_v = false;

template <typename... Args>
inline constexpr bool is_print_pack_v<::fast_io::manipulators::pack_t<Args...>> = true;

template <typename T>
concept print_pack = is_print_pack_v<::std::remove_cvref_t<T>>;

} // namespace details

namespace details::decay
{

template <typename T>
inline constexpr bool print_semantic_condition_v = false;

template <typename T1, typename T2>
inline constexpr bool
	print_semantic_condition_v<::fast_io::manipulators::condition<T1, T2>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v = false;

template <::fast_io::manipulators::scalar_placement placement, typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_t<placement, T>> = true;

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
inline constexpr bool
	print_semantic_width_v<::fast_io::manipulators::width_ch_t<placement, T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_t<T>> = true;

template <typename T, ::std::integral ch_type>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_node_no_parameter_v =
	::fast_io::details::print_pack<T> ||
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_width_v<::std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v<::fast_io::parameter<T>> = true;

template <typename T>
inline constexpr bool print_semantic_parameter_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_v<::fast_io::parameter<T>> =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
concept print_semantic_node =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
struct print_semantic_width_traits
{};

template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_t<placement, T>>
{
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = false;
};

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = true;
};

template <typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_t<T>>
{
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = false;
};

template <typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = true;
};

template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single;

template <::std::integral char_type, typename T>
using print_semantic_forwarded_arg_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

/// @brief Models consumption of an ordinary forwarding result through its named local.
/// @details Raw pack-member normalization still selects `io_print_forward(io_print_alias(member))`; this alias changes
///          no CPO choice. It only records the subsequent expression category: a value result is an lvalue once bound
///          by the pack expander, while an lvalue-reference result retains its exact cv-qualification.
template <::std::integral char_type, typename T>
using print_semantic_stable_forwarded_arg_t =
	::std::add_lvalue_reference_t<::std::remove_reference_t<
		::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>>;

/// @brief Models raw character-aware forwarding from a stored semantic pack member.
/// @details Pack expansion tests whether a named member is itself a pack and otherwise invokes the ordinary
///          `io_print_forward(io_print_alias(member))` expression. This helper deliberately retains that raw behavior;
///          condition alternatives and width children use the semantic-input resolver below, whose treatment of an
///          alias result that is already a semantic node is observably different.
/// @tparam   char_type the output character type
/// @tparam   T         the declared stored-member type
template <::std::integral char_type, typename T>
using print_semantic_named_member_forwarded_arg_t =
	print_semantic_forwarded_arg_t<char_type, T &>;

template <::std::integral char_type, typename T>
using print_semantic_input_alias_result_t =
	decltype(::fast_io::io_print_alias(::std::declval<T>()));

template <::std::integral char_type, typename T,
		  bool alias_is_semantic_node =
			  ::fast_io::details::decay::print_semantic_node_no_parameter_v<
				  ::std::remove_cvref_t<
					  ::fast_io::details::decay::
						  print_semantic_input_alias_result_t<char_type, T>>>>
struct print_semantic_input_forwarded_arg_resolver
{
	using type = ::fast_io::details::decay::
		print_semantic_forwarded_arg_t<char_type, T>;
};

/// @brief Mirrors semantic input forwarding when aliasing has already produced a parameterless semantic node.
/// @details Runtime skips `status_io_print_forward` in this case and applies only the ordinary transport policy. The
///          separate specialization prevents a condition or width trait from proving a replacement CPO which execution
///          never invokes.
template <::std::integral char_type, typename T>
struct print_semantic_input_forwarded_arg_resolver<char_type, T, true>
{
	using type = decltype(::fast_io::details::io_print_forward_transport(
		::fast_io::io_print_alias(::std::declval<T>())));
};

template <::std::integral char_type, typename T>
using print_semantic_input_forwarded_arg_t =
	typename ::fast_io::details::decay::
		print_semantic_input_forwarded_arg_resolver<char_type, T>::type;

/// @brief Models the expression consumed after semantic input forwarding has completed.
/// @details Runtime binds the forwarding result to a named `decltype(auto)` local before sizing or emission. A value or
///          rvalue-reference result is therefore consumed as a mutable lvalue, while an lvalue-reference result retains
///          its referent's cv-qualification. This stabilization is observable for exact-category protocols such as
///          `scatter_printable_for`: proving the raw result category would admit an rvalue-only CPO which execution
///          never calls and reject the corresponding lvalue-only CPO which it does call.
template <::std::integral char_type, typename T>
using print_semantic_stable_input_forwarded_arg_t =
	::std::add_lvalue_reference_t<::std::remove_reference_t<
		::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
			char_type, T>>>;

/// @brief Exposes the raw semantic-input forwarding result for a named condition arm or width child.
/// @details The containing semantic node is a named lvalue, so its value members enter forwarding as `T&`. This alias
///          intentionally denotes the CPO result type for compatibility and introspection; strategy proofs use the
///          stable-expression alias below when they model the subsequent named local.
template <::std::integral char_type, typename T>
using print_semantic_named_member_input_forwarded_arg_t =
	::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
		char_type, T &>;

/// @brief Models the named lvalue expression consumed after semantic input normalization.
/// @details A value result is bound to a local before traversal, while an existing reference retains its referred-to
///          cv-qualification. This is the expression on which `print_semantic_node_ref` begins at runtime.
template <typename T>
using print_semantic_named_node_input_t =
	::std::add_lvalue_reference_t<::std::remove_reference_t<T>>;

template <typename T,
		  bool is_parameter =
			  ::fast_io::details::decay::print_semantic_parameter_object_v<
				  ::std::remove_cvref_t<T>>>
struct print_semantic_node_expression_resolver
{
	using type = ::fast_io::details::decay::print_semantic_named_node_input_t<T>;
};

/// @brief Reproduces recursive parameter unwrapping while retaining the wrapper expression's exact cv/ref state.
/// @details Runtime reads `reference` from a named wrapper and recursively unwraps another parameter if present. Using
///          the actual member-access expression preserves a stored `const pack&`; normalizing the node type here would
///          incorrectly turn every member of that pack into a mutable lvalue during capability proof.
template <typename T>
struct print_semantic_node_expression_resolver<T, true>
{
	using wrapper_expression =
		::fast_io::details::decay::print_semantic_named_node_input_t<T>;
	using member_expression =
		decltype((::std::declval<wrapper_expression>().reference));
	using type = typename ::fast_io::details::decay::
		print_semantic_node_expression_resolver<member_expression>::type;
};

template <typename T>
using print_semantic_node_expression_t =
	typename ::fast_io::details::decay::
		print_semantic_node_expression_resolver<T>::type;

/// @brief Adds two optional strategy metrics without making overflow a compilation error.
/// @details `SIZE_MAX` is the common "strategy unavailable" sentinel in the print policy layer.  Unlike output-size
///          arithmetic performed immediately before a real allocation, these metrics only decide whether an optional
///          optimization may be instantiated.  Saturating therefore preserves correctness: every comparison treats
///          the result as at least as large as either operand, while the caller can conservatively decline the plan.
/// @param left  the accumulated metric
/// @param right the next metric contribution
/// @return the exact sum when representable, otherwise SIZE_MAX
inline constexpr ::std::size_t print_strategy_saturating_add(::std::size_t left,
															 ::std::size_t right) noexcept
{
	if (SIZE_MAX - left < right)
	{
		return SIZE_MAX;
	}
	return left + right;
}

/// @brief Adds two character/descriptor extents, or rejects the optional contiguous plan.
/// @details A C++ array and every cursor difference used to traverse it must be representable by `ptrdiff_t`.  Thus a
///          merely non-wrapping `size_t` sum is insufficient for a coalesced print plan. This strategy policy is
///          deliberately stricter than the language limit: it admits only totals less than `PTRDIFF_MAX`, leaving the
///          boundary value unavailable even though that value itself is representable. The ordered tests precede
///          subtraction, proving that `extent_limit - 1u - right` cannot underflow; rejection lets the dispatcher emit
///          smaller independent runs instead.
/// @param left  the accumulated contiguous extent
/// @param right the next extent contribution
/// @return the exact policy-admitted extent, otherwise SIZE_MAX
inline constexpr ::std::size_t print_strategy_extent_add_or_unavailable(::std::size_t left,
																		::std::size_t right) noexcept
{
	constexpr ::std::size_t extent_limit{static_cast<::std::size_t>(PTRDIFF_MAX)};
	if (extent_limit <= left || extent_limit <= right || extent_limit - 1u - right < left)
	{
		return SIZE_MAX;
	}
	return left + right;
}

/// @brief Adds two strategy hints while enforcing an inclusive policy cap.
/// @details The cap is a storage budget, so all values at or above it are equivalent to the optimizer.  Testing each
///          operand before subtracting proves that `cap - left` is defined; testing `cap - left <= right` then proves
///          the final addition cannot overflow and never exceeds the cap.
/// @param left  the accumulated bounded hint
/// @param right the next hint contribution
/// @param cap   the maximum useful strategy value
/// @return min(cap, left + right), computed without overflowing size_t
inline constexpr ::std::size_t print_strategy_add_capped(::std::size_t left, ::std::size_t right,
														 ::std::size_t cap) noexcept
{
	if (cap == 0u || cap <= left || cap <= right || cap - left <= right)
	{
		return cap;
	}
	return left + right;
}

/// @brief    Counts the maximum number of emitted leaves represented by a semantic print type.
/// @details  Ordinary values contribute one leaf. Semantic node specializations recursively describe their active
///           output shape so strategy selection can distinguish compact compositions from long materializations.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_leaf_count_impl : ::std::integral_constant<::std::size_t, 1u>
{};

/// @brief    Normalizes cv-reference qualifiers before computing a semantic output leaf count.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_leaf_count
	: ::fast_io::details::decay::print_semantic_leaf_count_impl<::std::remove_cvref_t<T>>
{};

/// @brief    Propagates semantic leaf counting through a parameter wrapper.
/// @tparam   T the wrapped semantic type
template <typename T>
struct print_semantic_leaf_count_impl<::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Sums semantic leaf counts, saturating when the optional cost metric overflows.
/// @tparam   Args the semantic types whose leaf counts are accumulated
/// @return   ::std::size_t the total semantic leaf count, or SIZE_MAX when it is not representable
template <typename... Args>
inline consteval ::std::size_t print_semantic_leaf_count_sum() noexcept
{
	::std::size_t total{};
	((total = ::fast_io::details::decay::print_strategy_saturating_add(
		  total, ::fast_io::details::decay::print_semantic_leaf_count<Args>::value)),
	 ...);
	return total;
}

/// @brief    Sums the semantic leaf counts of every stored pack element.
/// @tparam   Args the stored pack element types
template <typename... Args>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::pack_t<Args...>>
	: ::std::integral_constant<::std::size_t,
							   ::fast_io::details::decay::print_semantic_leaf_count_sum<Args...>()>
{};

/// @brief    Uses the larger alternative as a condition node's compile-time leaf-count bound.
/// @tparam   T1 the true alternative type
/// @tparam   T2 the false alternative type
template <typename T1, typename T2>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::condition<T1, T2>>
	: ::std::integral_constant<
		  ::std::size_t,
		  (::fast_io::details::decay::print_semantic_leaf_count<T1>::value <
		   ::fast_io::details::decay::print_semantic_leaf_count<T2>::value)
			  ? ::fast_io::details::decay::print_semantic_leaf_count<T2>::value
			  : ::fast_io::details::decay::print_semantic_leaf_count<T1>::value>
{};

/// @brief    Propagates semantic leaf counting through a statically placed width node.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a statically placed width node with an explicit fill character.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
/// @tparam   ch_type   the fill character type
template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a run-time placed width node.
/// @tparam   T the formatted child type
template <typename T>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a run-time placed width node with an explicit fill character.
/// @tparam   T       the formatted child type
/// @tparam   ch_type the fill character type
template <typename T, ::std::integral ch_type>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief  Maximum leaf count for selecting a separate run-time precise-size traversal.
/// @details Eight-leaf and larger statically bounded compositions favor one-pass bounded materialization according to
///          concat, fake-system-call, null-device, and file-sink benchmarks.
inline constexpr ::std::size_t print_semantic_precise_materialization_leaf_threshold{8u};

/// @brief    Detects whether a semantic print type contains width formatting.
/// @details  Width-bearing compositions use a distinct no-coalescing strategy because exact measurement can otherwise
///           be repeated by both the enclosing semantic run and the width dispatcher.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_contains_width_impl : ::std::false_type
{};

/// @brief    Normalizes cv-reference qualifiers before detecting width formatting.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_contains_width
	: ::fast_io::details::decay::print_semantic_contains_width_impl<::std::remove_cvref_t<T>>
{};

/// @brief    Propagates width detection through a parameter wrapper.
/// @tparam   T the wrapped semantic type
template <typename T>
struct print_semantic_contains_width_impl<::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_contains_width<T>
{};

/// @brief    Detects width formatting in any stored pack element.
/// @tparam   Args the stored pack element types
template <typename... Args>
struct print_semantic_contains_width_impl<::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_contains_width<Args>::value || ...)>
{};

/// @brief    Detects width formatting in either condition alternative.
/// @tparam   T1 the true alternative type
/// @tparam   T2 the false alternative type
template <typename T1, typename T2>
struct print_semantic_contains_width_impl<::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_contains_width<T1>::value ||
		  ::fast_io::details::decay::print_semantic_contains_width<T2>::value>
{};

/// @brief    Marks a statically placed width node as width-bearing.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_t<placement, T>> : ::std::true_type
{};

/// @brief    Marks a statically placed width node with an explicit fill character as width-bearing.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
/// @tparam   ch_type   the fill character type
template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::true_type
{};

/// @brief    Marks a run-time placed width node as width-bearing.
/// @tparam   T the formatted child type
template <typename T>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_runtime_t<T>> : ::std::true_type
{};

/// @brief    Marks a run-time placed width node with an explicit fill character as width-bearing.
/// @tparam   T       the formatted child type
/// @tparam   ch_type the fill character type
template <typename T, ::std::integral ch_type>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::true_type
{};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl
{
	inline static constexpr bool available = ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;
	inline static constexpr ::std::size_t size = available ? 0u : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size
	: ::fast_io::details::decay::print_semantic_static_precise_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
	requires(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
			 ::fast_io::static_precise_reserve_printable<char_type, T>)
struct print_semantic_static_precise_size_impl<char_type, T>
{
	inline static constexpr bool available = true;
	inline static constexpr ::std::size_t size{
		print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>)};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_precise_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool components_available =
		(::fast_io::details::decay::print_semantic_static_precise_size<
			 char_type, ::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	// The constexpr data member below is the proof boundary; the helper need not be an immediate function. This spelling
	// preserves mandatory constant evaluation while remaining accepted by early conforming C++20 frontends.
	static constexpr ::std::size_t static_size() noexcept
	{
		if constexpr (components_available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::decay::print_strategy_extent_add_or_unavailable(
				  total, ::fast_io::details::decay::print_semantic_static_precise_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
	// An overflowing nested pack is not a statically precise coalescing candidate.  Marking it unavailable is essential:
	// two equal SIZE_MAX sentinels must not make a condition node appear to have equal exact branch sizes.
	inline static constexpr bool available{components_available && size != SIZE_MAX};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::
					   print_semantic_named_member_input_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::
					   print_semantic_named_member_input_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available =
		first_size::available && second_size::available && (first_size::size == second_size::size);
	inline static constexpr ::std::size_t size = available ? first_size::size : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size_impl
{
	inline static constexpr bool available =
		::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
		(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
		 (::fast_io::static_precise_reserve_printable<char_type, T> ||
		  ::fast_io::reserve_printable<char_type, T>));

	inline static constexpr ::std::size_t static_size() noexcept
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			return 0u;
		}
		else if constexpr (!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
						   ::fast_io::static_precise_reserve_printable<char_type, T>)
		{
			return print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>);
		}
		else if constexpr (!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
						   ::fast_io::reserve_printable<char_type, T>)
		{
			return print_reserve_size(::fast_io::io_reserve_type<char_type, T>);
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size
	: ::fast_io::details::decay::print_semantic_static_bounded_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_bounded_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool components_available =
		(::fast_io::details::decay::print_semantic_static_bounded_size<
			 char_type, ::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	inline static constexpr ::std::size_t static_size() noexcept
	{
		if constexpr (components_available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::decay::print_strategy_extent_add_or_unavailable(
				  total, ::fast_io::details::decay::print_semantic_static_bounded_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
	// SIZE_MAX denotes that the aggregate cannot back one contiguous object; callers may still emit each child normally.
	inline static constexpr bool available{components_available && size != SIZE_MAX};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_bounded_size<
		char_type, ::fast_io::details::decay::
					   print_semantic_named_member_input_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_bounded_size<
		char_type, ::fast_io::details::decay::
					   print_semantic_named_member_input_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available = first_size::available && second_size::available;

	inline static constexpr ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			if constexpr (first_size::size < second_size::size)
			{
				return second_size::size;
			}
			else
			{
				return first_size::size;
			}
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_precise_leaf_size_ok_v =
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
	::fast_io::static_precise_reserve_printable<char_type, T> ||
	::fast_io::precise_reserve_printable<char_type, T> ||
	(::fast_io::scatter_printable_for<char_type, T> &&
	 ::fast_io::borrowed_scatter_source<char_type, ::std::remove_cvref_t<T>>);

// A scatter length is exact for one observation, but the shape-only scatter protocol does not make a second
// observation equivalent to the first. Semantic precise sizing measures a leaf and later invokes its CPO again while
// materializing the allocation; the bounded width path performs the same replay whenever no reserve bound exists.
// `borrowed_scatter_source` is deliberately the additional proof here: besides retaining the pointed-to characters,
// its public contract promises that an unchanged source yields the same length and sequence throughout one logical
// print. Requiring the exact `T` expression above and the decayed source marker here prevents both ref-category drift
// and a consuming/shared-scratch producer from turning the first length into an unsafe capacity for the second call.

// A reserve-scatters capacity is not a side-effect-free size query. Obtaining its precise payload length requires
// invoking `print_reserve_scatters_define`; admitting that leaf here would invoke the producer once during semantic
// sizing and again during emission. Neither the shape concept nor `borrowed_reserve_scatters_source` proves replay,
// purity, or stable output--the borrowed marker proves only descriptor lifetime across later producer calls. Such
// leaves therefore remain on single-pass stream/concat plans unless a future, independent repeatability protocol is
// introduced. The bounded trait below inherits this exclusion through `print_semantic_precise_leaf_size_ok_v`.

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_dispatch
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok;

template <::std::integral char_type, typename node_expression, ::std::size_t... I>
inline consteval bool print_semantic_precise_pack_expression_ok(
	::std::index_sequence<I...>) noexcept
{
	return (true && ... &&
			::fast_io::details::decay::print_semantic_precise_size_ok<
				char_type,
				::fast_io::details::decay::print_semantic_stable_forwarded_arg_t<
					char_type,
					decltype(::fast_io::containers::get<I>(
						::std::declval<node_expression>().storage))>>::value);
}

/// @brief Proves precise sizing against the exact named semantic-node expression traversed at runtime.
/// @details Structural recognition uses the normalized node type, but every child expression is derived from the
///          cv-qualified node reference. Pack members keep the established raw forwarding policy; condition and width
///          children use semantic-input forwarding, exactly matching their runtime consumers.
template <::std::integral char_type, typename T>
inline consteval bool print_semantic_precise_node_expression_ok() noexcept
{
	using node_expression =
		::fast_io::details::decay::print_semantic_node_expression_t<T>;
	using node_type = ::std::remove_cvref_t<node_expression>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		return ::fast_io::details::decay::
			print_semantic_precise_pack_expression_ok<char_type, node_expression>(
				::std::make_index_sequence<node_type::size>{});
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		using first_expression =
			decltype((::std::declval<node_expression>().t1));
		using second_expression =
			decltype((::std::declval<node_expression>().t2));
		return ::fast_io::details::decay::print_semantic_precise_size_ok<
				   char_type,
				   ::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
					   char_type, first_expression>>::value &&
			   ::fast_io::details::decay::print_semantic_precise_size_ok<
				   char_type,
				   ::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
					   char_type, second_expression>>::value;
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		using width_traits =
			::fast_io::details::decay::print_semantic_width_traits<node_type>;
		if constexpr (width_traits::has_fill_char)
		{
			if constexpr (!::std::same_as<
							  char_type, typename width_traits::fill_char_type>)
			{
				return false;
			}
		}
		using child_expression =
			decltype((::std::declval<node_expression>().reference));
		return ::fast_io::details::decay::print_semantic_precise_size_ok<
			char_type,
			::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
				char_type, child_expression>>::value;
	}
	else
	{
		return false;
	}
}

/// @brief Routes structural semantic nodes while preserving the exact node expression used by sizing.
/// @details Normalization is sufficient only to identify node shape. The expression-aware proof derives every member
///          from the named node reference, so a `const pack&` cannot advertise mutable-only leaf sizing.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::decay::print_semantic_node<T>
struct print_semantic_precise_size_ok_dispatch<char_type, T>
	: ::std::bool_constant<
		  ::fast_io::details::decay::
			  print_semantic_precise_node_expression_ok<char_type, T>()>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok
	: ::fast_io::details::decay::print_semantic_precise_size_ok_dispatch<char_type, T>
{};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_bounded_leaf_size_ok_v =
	::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T> ||
	::fast_io::dynamic_reserve_printable<char_type, T> ||
	::fast_io::reserve_printable<char_type, T>;

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok_dispatch
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_bounded_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok;

template <::std::integral char_type, typename node_expression, ::std::size_t... I>
inline consteval bool print_semantic_bounded_pack_expression_ok(
	::std::index_sequence<I...>) noexcept
{
	return (true && ... &&
			::fast_io::details::decay::print_semantic_bounded_size_ok<
				char_type,
				::fast_io::details::decay::print_semantic_stable_forwarded_arg_t<
					char_type,
					decltype(::fast_io::containers::get<I>(
						::std::declval<node_expression>().storage))>>::value);
}

/// @brief Proves bounded sizing against the exact named semantic-node expression traversed at runtime.
template <::std::integral char_type, typename T>
inline consteval bool print_semantic_bounded_node_expression_ok() noexcept
{
	using node_expression =
		::fast_io::details::decay::print_semantic_node_expression_t<T>;
	using node_type = ::std::remove_cvref_t<node_expression>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		return ::fast_io::details::decay::
			print_semantic_bounded_pack_expression_ok<char_type, node_expression>(
				::std::make_index_sequence<node_type::size>{});
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		using first_expression =
			decltype((::std::declval<node_expression>().t1));
		using second_expression =
			decltype((::std::declval<node_expression>().t2));
		return ::fast_io::details::decay::print_semantic_bounded_size_ok<
				   char_type,
				   ::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
					   char_type, first_expression>>::value &&
			   ::fast_io::details::decay::print_semantic_bounded_size_ok<
				   char_type,
				   ::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
					   char_type, second_expression>>::value;
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		using width_traits =
			::fast_io::details::decay::print_semantic_width_traits<node_type>;
		if constexpr (width_traits::has_fill_char)
		{
			if constexpr (!::std::same_as<
							  char_type, typename width_traits::fill_char_type>)
			{
				return false;
			}
		}
		using child_expression =
			decltype((::std::declval<node_expression>().reference));
		return ::fast_io::details::decay::print_semantic_bounded_size_ok<
			char_type,
			::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
				char_type, child_expression>>::value;
	}
	else
	{
		return false;
	}
}

/// @brief Preserves the exact named node expression while dispatching bounded semantic structure.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::decay::print_semantic_node<T>
struct print_semantic_bounded_size_ok_dispatch<char_type, T>
	: ::std::bool_constant<
		  ::fast_io::details::decay::
			  print_semantic_bounded_node_expression_ok<char_type, T>()>
{};

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok
	: ::fast_io::details::decay::print_semantic_bounded_size_ok_dispatch<char_type, T>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay;

template <::std::integral char_type, typename node_expression, ::std::size_t... I>
inline consteval bool print_semantic_pack_params_expression_ok(
	::std::index_sequence<I...>) noexcept
{
	return (true && ... &&
			::fast_io::details::decay::print_freestanding_decay_param_okay_single<
				char_type,
				::fast_io::details::decay::print_semantic_forwarded_arg_t<
					char_type,
					decltype(::fast_io::containers::get<I>(
						::std::declval<node_expression>().storage))>>::value);
}

/// @brief Proves destination-independent semantic compatibility on the exact named node expression.
/// @details The compatibility predicate is executable in legacy default-output dispatch, so it must retain an outer
///          node's cv-qualification just like the destination-aware proof. Pack members use raw forwarding, while
///          condition arms and width children use semantic-input forwarding. Recursively wrapped packs remain admitted
///          because runtime explicitly expands that shape; multiply wrapped conditions and width nodes remain outside
///          the proof because execution recognizes only one parameter layer for those node kinds.
template <::std::integral char_type, typename T>
inline consteval bool print_semantic_params_node_expression_ok() noexcept
{
	using node_expression =
		::fast_io::details::decay::print_semantic_node_expression_t<T>;
	using node_type = ::std::remove_cvref_t<node_expression>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		return ::fast_io::details::decay::
			print_semantic_pack_params_expression_ok<char_type, node_expression>(
				::std::make_index_sequence<node_type::size>{});
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		using first_expression =
			decltype((::std::declval<node_expression>().t1));
		using second_expression =
			decltype((::std::declval<node_expression>().t2));
		return ::fast_io::details::decay::
				   print_freestanding_decay_param_okay_single<
					   char_type,
					   ::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
						   char_type, first_expression>>::value &&
			   ::fast_io::details::decay::
				   print_freestanding_decay_param_okay_single<
					   char_type,
					   ::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
						   char_type, second_expression>>::value;
	}
	else if constexpr (
		::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		using width_traits =
			::fast_io::details::decay::print_semantic_width_traits<node_type>;
		if constexpr (width_traits::has_fill_char)
		{
			if constexpr (!::std::same_as<
							  char_type, typename width_traits::fill_char_type>)
			{
				return false;
			}
		}
		using child_expression =
			decltype((::std::declval<node_expression>().reference));
		return ::fast_io::details::decay::
			print_freestanding_decay_param_okay_single<
				char_type,
				::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
					char_type, child_expression>>::value;
	}
	else
	{
		return false;
	}
}

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_params_node_v =
	::fast_io::details::decay::print_semantic_node<T> ||
	::fast_io::details::print_pack<::std::remove_cvref_t<
		::fast_io::details::decay::print_semantic_node_expression_t<T>>>;

template <::std::integral char_type, typename T>
struct print_semantic_params_okay_dispatch : ::std::false_type
{};

template <::std::integral char_type, typename T>
	requires ::fast_io::details::decay::print_semantic_params_node_v<char_type, T>
struct print_semantic_params_okay_dispatch<char_type, T>
	: ::std::bool_constant<
		  ::fast_io::details::decay::
			  print_semantic_params_node_expression_ok<char_type, T>()>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay
	: ::fast_io::details::decay::print_semantic_params_okay_dispatch<char_type, T>
{};

} // namespace details::decay

} // namespace fast_io

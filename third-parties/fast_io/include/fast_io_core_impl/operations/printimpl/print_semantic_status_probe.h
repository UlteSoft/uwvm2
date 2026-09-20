#pragma once

// Type-only proof of exact active-record status ownership, included inside namespace fast_io::operations::decay
// after the semantic normalization helpers are declared. This header neither evaluates conditions nor emits output.

template <typename... Types>
struct print_semantic_status_probe_types
{};

template <::std::integral char_type, typename T>
struct print_semantic_status_probe_closed_leaf : ::std::false_type
{};

template <::std::integral char_type, ::std::size_t extent>
struct print_semantic_status_probe_closed_leaf<
	char_type, ::fast_io::manipulators::static_scatter_t<char_type, extent>> : ::std::true_type
{};

template <::std::integral char_type>
struct print_semantic_status_probe_closed_leaf<
	char_type, ::fast_io::manipulators::chvw_t<char_type>> : ::std::true_type
{};

/// @brief Admits only the closed literal condition graph whose branch normalization is modeled below.
/// @details Mandatory arguments retain their already-normalized expression types. A condition arm is either another
///          condition, exact null, or an exact fast_io static-scatter/character value. Packs, widths, custom arms, and
///          volatile expressions fail closed; this is a scope restriction rather than a claim about their CPO owners.
template <::std::integral char_type, typename Expression, bool condition_arm = false>
inline consteval bool print_semantic_status_probe_source_available() noexcept
{
	if constexpr (::std::is_volatile_v<::std::remove_reference_t<Expression>>)
	{
		return false;
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Expression>, ::fast_io::io_null_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_top_level_condition_v<Expression>)
	{
		using node_result = decltype(::fast_io::details::decay::print_semantic_node_ref(
			::std::declval<Expression>()));
		using node_expression = ::std::add_lvalue_reference_t<::std::remove_reference_t<node_result>>;
		return ::fast_io::operations::decay::print_semantic_status_probe_source_available<
				   char_type, decltype((::std::declval<node_expression>().t1)), true>() &&
			   ::fast_io::operations::decay::print_semantic_status_probe_source_available<
				   char_type, decltype((::std::declval<node_expression>().t2)), true>();
	}
	else if constexpr (condition_arm)
	{
		if constexpr (!::fast_io::operations::decay::print_semantic_status_probe_closed_leaf<
						  char_type, ::std::remove_cvref_t<Expression>>::value)
		{
			return false;
		}
		else
		{
			using forwarded_result = decltype(::fast_io::details::decay::print_semantic_input_forward<char_type>(
				::std::declval<Expression>()));
			// Only the literal's existing value transport is modeled here. A provider replacement, including a null
			// returned by forwarding rather than a raw null arm, may have different normalization boundaries.
			return ::std::same_as<forwarded_result, ::std::remove_cvref_t<Expression>>;
		}
	}
	else
	{
		return !::fast_io::details::decay::print_semantic_execution_node_v<Expression>;
	}
}

template <bool condition_arm, typename Expression>
struct print_semantic_status_probe_input
{};

template <bool line, ::std::integral char_type, typename output, typename active, typename remaining>
struct print_semantic_status_probe_any_impl;

/// @brief Queries only the exact terminal status protocol, without instantiating ordinary output strategies.
template <bool line, ::std::integral char_type, typename output, typename... Active>
struct print_semantic_status_probe_any_impl<
	line, char_type, output, print_semantic_status_probe_types<Active...>, print_semantic_status_probe_types<>>
	: ::std::bool_constant<::fast_io::operations::decay::defines::has_status_print_define<
		  line, output, ::std::remove_reference_t<Active>...>>
{};

/// @brief Enumerates exact active types while leaving every runtime predicate and object untouched.
/// @details Template specialization identity shares equal active prefixes with equal remaining tails. The worst case
///          still has one query per distinct active type sequence, as an arbitrary ADL status owner may distinguish
///          each of those sequences. Explicit if-constexpr short circuit stops as soon as any owner is found.
template <bool line, ::std::integral char_type, typename output, typename... Active,
		  bool condition_arm, typename Expression, typename... Tail>
struct print_semantic_status_probe_any_impl<
	line, char_type, output, print_semantic_status_probe_types<Active...>,
	print_semantic_status_probe_types<print_semantic_status_probe_input<condition_arm, Expression>, Tail...>>
{
	inline static constexpr bool value{[]() consteval {
		using active = print_semantic_status_probe_types<Active...>;
		using tail = print_semantic_status_probe_types<Tail...>;
		if constexpr (::std::same_as<::std::remove_cvref_t<Expression>, ::fast_io::io_null_t>)
		{
			return print_semantic_status_probe_any_impl<line, char_type, output, active, tail>::value;
		}
		else if constexpr (::fast_io::details::decay::print_semantic_top_level_condition_v<Expression>)
		{
			using node_result = decltype(::fast_io::details::decay::print_semantic_node_ref(
				::std::declval<Expression>()));
			using node_expression = ::std::add_lvalue_reference_t<::std::remove_reference_t<node_result>>;
			using first = print_semantic_status_probe_types<
				print_semantic_status_probe_input<true, decltype((::std::declval<node_expression>().t1))>, Tail...>;
			using second = print_semantic_status_probe_types<
				print_semantic_status_probe_input<true, decltype((::std::declval<node_expression>().t2))>, Tail...>;
			if constexpr (print_semantic_status_probe_any_impl<line, char_type, output, active, first>::value)
			{
				return true;
			}
			else
			{
				return print_semantic_status_probe_any_impl<line, char_type, output, active, second>::value;
			}
		}
		else if constexpr (condition_arm)
		{
			// The ordinary branch selector owns the forwarding result in a named local. Preserve that exact operation
			// and its cv-qualified result expression in the proof; do not infer the result solely from the raw leaf type.
			using forwarded_result = decltype(::fast_io::details::decay::print_semantic_input_forward<char_type>(
				::std::declval<Expression>()));
			using forwarded_expression = ::std::add_lvalue_reference_t<::std::remove_reference_t<forwarded_result>>;
			return print_semantic_status_probe_any_impl<line, char_type, output,
														print_semantic_status_probe_types<Active..., forwarded_expression>, tail>::value;
		}
		else
		{
			return print_semantic_status_probe_any_impl<line, char_type, output,
														print_semantic_status_probe_types<Active..., Expression>, tail>::value;
		}
	}()};
};

#if __cplusplus > 202302L && __cpp_pack_indexing >= 202311L

/// @brief Identifies the one-level condition graph represented by the scalar choice table.
template <typename Expression>
inline consteval bool print_semantic_status_probe_flat_source() noexcept
{
	if constexpr (::fast_io::details::decay::print_semantic_top_level_condition_v<Expression>)
	{
		using node_result = decltype(::fast_io::details::decay::print_semantic_node_ref(
			::std::declval<Expression>()));
		using node_expression = ::std::add_lvalue_reference_t<::std::remove_reference_t<node_result>>;
		return !::fast_io::details::decay::print_semantic_top_level_condition_v<
				   decltype((::std::declval<node_expression>().t1))> &&
			   !::fast_io::details::decay::print_semantic_top_level_condition_v<
				   decltype((::std::declval<node_expression>().t2))>;
	}
	else
	{
		return true;
	}
}

template <::std::integral char_type, typename Expression,
		  bool is_null = ::std::same_as<::std::remove_cvref_t<Expression>, ::fast_io::io_null_t>>
struct print_semantic_status_probe_arm_expression
{
	using result = decltype(::fast_io::details::decay::print_semantic_input_forward<char_type>(
		::std::declval<Expression>()));
	using type = ::std::add_lvalue_reference_t<::std::remove_reference_t<result>>;
};

template <::std::integral char_type, typename Expression>
struct print_semantic_status_probe_arm_expression<char_type, Expression, true>
{
	using type = ::fast_io::io_null_t &;
};

template <::std::integral char_type, typename Expression,
		  bool is_condition = ::fast_io::details::decay::print_semantic_top_level_condition_v<Expression>>
struct print_semantic_status_probe_source_expressions
{
	using first = Expression;
	using second = Expression;
	inline static constexpr bool has_choice{false};

	template <::std::size_t>
	using expression = Expression;
};

template <::std::integral char_type, typename Expression>
struct print_semantic_status_probe_source_expressions<char_type, Expression, true>
{
	using node_result = decltype(::fast_io::details::decay::print_semantic_node_ref(
		::std::declval<Expression>()));
	using node_expression = ::std::add_lvalue_reference_t<::std::remove_reference_t<node_result>>;
	using first = typename print_semantic_status_probe_arm_expression<
		char_type, decltype((::std::declval<node_expression>().t1))>::type;
	using second = typename print_semantic_status_probe_arm_expression<
		char_type, decltype((::std::declval<node_expression>().t2))>::type;
	// Equal normalized expressions have identical status lookup, irrespective of the selected arm's runtime value.
	inline static constexpr bool has_choice{!::std::same_as<first, second>};

	template <::std::size_t alternative>
	using expression = ::std::conditional_t<alternative == 0u, first, second>;
};

struct print_semantic_status_probe_absent
{};

template <typename Expression>
using print_semantic_status_probe_canonical_expression = ::std::conditional_t<
	::std::same_as<::std::remove_cvref_t<Expression>, ::fast_io::io_null_t>,
	print_semantic_status_probe_absent, ::std::remove_reference_t<Expression>>;

namespace defines
{

/// @brief Shares the status proof by its canonical terminal types rather than by raw condition storage wrappers.
/// @details The bank stores all first-arm types followed by all second-arm types. References are removed exactly as
///          in has_status_print_define, while const and other terminal type distinctions remain intact. Therefore
///          source graphs with the same normalized alternatives reuse the whole proof, even if their original
///          condition objects differed in const qualification or parameter storage. Each terminal uses one direct
///          language pack index per selected type, without redoing source-trait and forwarding aliases per mask.
template <bool line, typename output, ::std::size_t source_count, typename... Types>
struct print_semantic_status_probe_canonical_graph
{
	template <::std::size_t index>
	using expression = Types...[index];

	struct source_metadata
	{
		bool has_choice;
		bool first_null;
		bool second_null;
	};

	struct source_table
	{
		source_metadata elements[source_count + 1u]{};
	};

	template <::std::size_t... I>
	static consteval source_table make_sources(::std::index_sequence<I...>) noexcept
	{
		source_table result{};
		((result.elements[I] = {
			  !::std::same_as<expression<I>, expression<source_count + I>>,
			  ::std::same_as<expression<I>, print_semantic_status_probe_absent>,
			  ::std::same_as<expression<source_count + I>, print_semantic_status_probe_absent>}),
		 ...);
		return result;
	}

	inline static constexpr source_table sources{make_sources(::std::make_index_sequence<source_count>{})};

	inline static constexpr ::std::size_t choice_count{[]() consteval {
		::std::size_t result{};
		for (::std::size_t index{}; index != source_count; ++index)
		{
			result += static_cast<::std::size_t>(sources.elements[index].has_choice);
		}
		return result;
	}()};

	struct selection
	{
		::std::size_t indices[source_count + 1u]{};
		::std::size_t count{};
	};

	static consteval selection make_selection(::std::size_t mask) noexcept
	{
		selection result{};
		for (::std::size_t source_index{}; source_index != source_count; ++source_index)
		{
			auto const source{sources.elements[source_index]};
			bool second{};
			if (source.has_choice)
			{
				second = (mask & 1u) != 0u;
				mask >>= 1u;
			}
			if (!(second ? source.second_null : source.first_null))
			{
				result.indices[result.count++] = source_index + (second ? source_count : 0u);
			}
		}
		return result;
	}

	template <::std::size_t mask>
	inline static constexpr selection selected{make_selection(mask)};

	template <::std::size_t mask, ::std::size_t... I>
	static consteval bool query(::std::index_sequence<I...>) noexcept
	{
		// Repeat has_status_print_define's named requires-expression in the same defines namespace. The exact
		// parameter types preserve cv qualification and array/function adjustments, and therefore the original
		// lvalue expressions, associated ADL namespaces, overload resolution, and required void return type.
		return requires(output optstm, expression<selected<mask>.indices[I]>... args) {
			{ status_print_define<line>(optstm, args...) } -> ::std::same_as<void>;
		};
	}

	template <::std::size_t begin, ::std::size_t... I>
	static consteval bool query_block(::std::index_sequence<I...>) noexcept
	{
		return (query<begin + I>(::std::make_index_sequence<selected<begin + I>.count>{}) || ... || false);
	}

	template <::std::size_t begin, ::std::size_t count>
	static consteval bool query_range() noexcept
	{
		if constexpr (count <= 64u)
		{
			return query_block<begin>(::std::make_index_sequence<count>{});
		}
		else if constexpr (query_range<begin, count / 2u>())
		{
			return true;
		}
		else
		{
			return query_range<begin + count / 2u, count - count / 2u>();
		}
	}

	static consteval bool any() noexcept
	{
		if constexpr (choice_count >= static_cast<::std::size_t>(::std::numeric_limits<::std::size_t>::digits))
		{
			// A scalar mask cannot encode this graph. Conservatively retain the original complete dispatcher.
			return true;
		}
		else
		{
			return query_range<0u, static_cast<::std::size_t>(1u) << choice_count>();
		}
	}
};

} // namespace defines

template <bool line, ::std::integral char_type, typename output, typename... Args>
using print_semantic_status_probe_mask_graph =
	defines::print_semantic_status_probe_canonical_graph<line, output, sizeof...(Args),
														 print_semantic_status_probe_canonical_expression<
															 typename print_semantic_status_probe_source_expressions<char_type, Args &>::first>...,
														 print_semantic_status_probe_canonical_expression<
															 typename print_semantic_status_probe_source_expressions<char_type, Args &>::second>...>;

#endif

/// @brief Conservatively detects whether any selected literal condition record can own an exact status CPO.
/// @details Call only after the ordinary outer source-record status dispatch, using the effective unlocked output and
///          already-normalized named arguments. False proves absence for every possible active record, including the
///          empty record and the requested line policy. True also covers unsupported input shapes and destinations.
///          A caller may use a separately proved shared emitter only in the false branch. The true branch must retain
///          the complete original selector and dispatcher. There is no runtime probe, so predicate reads, branch
///          forwarding, and lifetime ownership are performed exactly once by whichever emitter the caller selects.
template <bool line, ::std::integral char_type, typename output, typename... Args>
inline consteval bool print_semantic_any_exact_status() noexcept
{
	using normalized_output = ::std::remove_reference_t<output>;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<normalized_output> ||
				  !(::fast_io::operations::decay::print_semantic_status_probe_source_available<char_type, Args &>() && ...))
	{
		return true;
	}
	else
	{
#if __cplusplus > 202302L && __cpp_pack_indexing >= 202311L
		if constexpr ((::fast_io::operations::decay::print_semantic_status_probe_flat_source<Args &>() && ...))
		{
			return print_semantic_status_probe_mask_graph<line, char_type, normalized_output, Args...>::any();
		}
		else
#endif
		{
			return print_semantic_status_probe_any_impl<line, char_type, normalized_output,
														print_semantic_status_probe_types<>,
														print_semantic_status_probe_types<print_semantic_status_probe_input<false, Args &>...>>::value;
		}
	}
}

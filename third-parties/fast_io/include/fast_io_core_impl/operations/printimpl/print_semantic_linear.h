#pragma once

// Shared control emission for closed semantic conditions. Include inside fast_io::operations::decay
// after semantic normalization helpers and the ordinary control emitters. The
// caller owns exact status dispatch, mutex handling, and admission of this path.
namespace print_semantic_linear
{

namespace d = ::fast_io::details::decay;

enum class capture_kind : unsigned char
{
	stop,
	reserve,
	dynamic,
	context,
	empty
};
enum class scatter_kind : unsigned char
{
	stop,
	reserve,
	dynamic,
	scatter,
	empty
};

struct description
{
	capture_kind capture{capture_kind::stop};
	scatter_kind scatter{scatter_kind::stop};
	::std::size_t reserve_size{};
	::std::size_t context_size{};
	::std::size_t dynamic_hint{};
	bool active{true};
	bool static_fragment{};
};

template <::std::integral Char, typename T>
struct closed_static_leaf : ::std::false_type
{};
template <::std::integral Char, ::std::size_t Size>
struct closed_static_leaf<Char, ::fast_io::manipulators::static_scatter_t<Char, Size>> : ::std::true_type
{};
template <::std::integral Char>
struct closed_static_leaf<Char, ::fast_io::manipulators::chvw_t<Char>> : ::std::true_type
{};

template <typename T>
inline consteval ::std::size_t selection_count()
{
	if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		using node = ::std::remove_reference_t<decltype(d::print_semantic_node_ref(::std::declval<T &>()))>;
		return selection_count<decltype(::std::declval<node &>().t1)>() +
			   selection_count<decltype(::std::declval<node &>().t2)>();
	}
	else
	{
		return 1u;
	}
}

template <::std::integral Char, typename T>
inline consteval bool optional_source_available()
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_volatile_v<::std::remove_reference_t<T>>)
	{
		return false;
	}
	else if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		using node = ::std::remove_reference_t<decltype(d::print_semantic_node_ref(::std::declval<T &>()))>;
		return optional_source_available<Char, decltype(::std::declval<node &>().t1)>() &&
			   optional_source_available<Char, decltype(::std::declval<node &>().t2)>();
	}
	else
	{
		if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
		{
			return true;
		}
		else if constexpr (closed_static_leaf<Char, value_type>::value)
		{
			// The ordinary branch selector materializes this exact ABI-small
			// value before any formatter executes. Cache that value, not a
			// reference to mutable condition-owned fields.
			return ::fast_io::reserve_printable<Char, value_type> &&
				   ::std::same_as<decltype(d::print_semantic_input_forward<Char>(::std::declval<T &>())), value_type>;
		}
		else
		{
			return false;
		}
	}
}

template <::std::integral Char, typename Output, typename T>
inline consteval bool plan_source_available()
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_volatile_v<::std::remove_reference_t<T>>)
	{
		return false;
	}
	else if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		return optional_source_available<Char, T>();
	}
	else if constexpr (d::print_semantic_execution_node_v<T> ||
					   ::fast_io::reserve_scatters_printable<Char, value_type> ||
					   ::fast_io::dynamic_reserve_scatters_printable<Char, value_type>)
	{
		return false;
	}
	else if constexpr ((::fast_io::dynamic_reserve_printable<Char, value_type> &&
						!::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>) ||
					   (::fast_io::reserve_printable<Char, value_type> && ::fast_io::dynamic_reserve_printable<Char, value_type>))
	{
		// A direct fallback must not bypass a competing dynamic reserve CPO.
		// Static/dynamic dual providers also have distinct measurement rules.
		return false;
	}
	else if constexpr (d::print_static_scatter_traits<Char, value_type>::available &&
					   !closed_static_leaf<Char, value_type>::value)
	{
		// Compiler-constant/static-provider proxies have additional policy and
		// provenance rules; this plan leaves them to the caller.
		return false;
	}
	else if constexpr (d::retained_scatter_printable_v<Char, T &> &&
					   (::fast_io::reserve_printable<Char, value_type> ||
						::fast_io::dynamic_reserve_printable<Char, value_type>))
	{
		// The ordinary scanner and mixed materializer prioritize these dual
		// protocols differently. Do not silently choose one representation.
		return false;
	}
	else
	{
		return ::std::same_as<value_type, ::fast_io::io_null_t> ||
			   ::fast_io::reserve_printable<Char, value_type> ||
			   d::retained_scatter_printable_v<Char, T &> ||
			   ::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type> ||
			   ::fast_io::context_printable_with_static_buffer_size<Char, value_type> ||
			   ::fast_io::details::direct_printable_to<Char, Output, value_type>;
	}
}

template <::std::integral Char, typename Output, typename T>
inline consteval description leaf_description()
{
	using value_type = ::std::remove_cvref_t<T>;
	description result;
	if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
	{
		return {capture_kind::empty, scatter_kind::empty, 0u, 0u, 0u, false, false};
	}
	if constexpr (::fast_io::reserve_printable<Char, value_type>)
	{
		result.capture = capture_kind::reserve;
		result.reserve_size = print_reserve_size(::fast_io::io_reserve_type<Char, value_type>);
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>)
	{
		result.capture = capture_kind::dynamic;
		result.dynamic_hint = d::dynamic_print_reserve_static_stack_budget<
			print_reserve_static_stack_size(::fast_io::io_reserve_type<Char, value_type>), Char>();
	}
	else if constexpr (::fast_io::context_printable_with_static_buffer_size<Char, value_type>)
	{
		result.capture = capture_kind::context;
		result.context_size = d::context_print_static_buffer_size_v<false, Char, value_type>;
	}
	if constexpr (d::retained_scatter_printable_v<Char, T &> ||
				  (d::print_output_retains_static_scatter<Output> && d::print_static_scatter_traits<Char, value_type>::available))
	{
		result.scatter = scatter_kind::scatter;
	}
	else if constexpr (::fast_io::reserve_printable<Char, value_type>)
	{
		result.scatter = scatter_kind::reserve;
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>)
	{
		result.scatter = scatter_kind::dynamic;
	}
	// Ordinary static_scatter_t retains its native category but does not
	// activate the compiler-constant-proxy preserving writer. Proxies are
	// outside this plan's admission, so the run flag remains false.
	result.static_fragment = false;
	return result;
}

template <typename T>
inline constexpr ::std::size_t select(T &source)
{
	if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		auto &&node{d::print_semantic_node_ref(source)};
		if (node.pred)
		{
			return select(node.t1);
		}
		return selection_count<decltype(node.t1)>() + select(node.t2);
	}
	else
	{
		return 0u;
	}
}

template <typename T, typename Function>
inline constexpr void visit_selected(T &source, ::std::size_t selection, Function &&function)
{
	if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		auto &&node{d::print_semantic_node_ref(source)};
		constexpr auto first_count{selection_count<decltype(node.t1)>()};
		if (selection < first_count)
		{
			visit_selected(node.t1, selection, function);
		}
		else
		{
			visit_selected(node.t2, selection - first_count, function);
		}
	}
	else
	{
		function(source);
	}
}

struct shape
{
	bool capture_compatible{};
	bool scatter_compatible{};
	bool context{};
	::std::size_t reserve_max{};
	::std::size_t context_max{};
	::std::size_t scratch_max{};
	::std::size_t dynamic_hint{};
	bool capture_resets_burst{};
};

template <::std::integral Char, typename Output, typename T>
inline consteval shape source_shape()
{
	if constexpr (d::print_semantic_top_level_condition_v<T &>)
	{
		using node = ::std::remove_reference_t<decltype(d::print_semantic_node_ref(::std::declval<T &>()))>;
		constexpr auto first{source_shape<Char, Output, decltype(::std::declval<node &>().t1)>()};
		constexpr auto second{source_shape<Char, Output, decltype(::std::declval<node &>().t2)>()};
		return {true, true, false, first.reserve_max < second.reserve_max ? second.reserve_max : first.reserve_max,
				0u, first.scratch_max < second.scratch_max ? second.scratch_max : first.scratch_max, 0u, false};
	}
	else
	{
		constexpr auto leaf{leaf_description<Char, Output, T>()};
		return {leaf.capture != capture_kind::stop, leaf.scatter != scatter_kind::stop,
				leaf.capture == capture_kind::context, leaf.reserve_size, leaf.context_size,
				leaf.scatter == scatter_kind::reserve ? leaf.reserve_size : 0u,
				leaf.dynamic_hint, leaf.capture == capture_kind::dynamic || leaf.capture == capture_kind::context};
	}
}

template <::std::size_t Index, typename T>
struct reference
{
	T *pointer;
};

template <::std::size_t Index, typename First, typename... Tail>
struct fallback_type_at : fallback_type_at<Index - 1u, Tail...>
{};
template <typename First, typename... Tail>
struct fallback_type_at<0u, First, Tail...>
{
	using type = First;
};

template <::std::integral Char>
struct selected_payload
{
	Char const *base{};
	Char character{};
};

template <::std::integral Char, typename Sequence, typename... Args>
struct source_graph;
template <::std::integral Char, ::std::size_t... Index, typename... Args>
struct source_graph<Char, ::std::index_sequence<Index...>, Args...> : reference<Index, Args>...
{
	using char_type = Char;
	selected_payload<Char> payloads[sizeof...(Args) == 0u ? 1u : sizeof...(Args)]{};
	inline constexpr explicit source_graph(Args &...args) : reference<Index, Args>{__builtin_addressof(args)}...
	{}
	template <::std::size_t Position>
	using source_type =
#if __cplusplus > 202302L && __cpp_pack_indexing >= 202311L
		Args...[Position];
#elif FAST_IO_HAS_BUILTIN(__type_pack_element)
		__type_pack_element<Position, Args...>;
#else
		typename fallback_type_at<Position, Args...>::type;
#endif
	template <::std::size_t Position>
	inline constexpr auto &get()
	{
		return *static_cast<reference<Position, source_type<Position>> &>(*this).pointer;
	}
};

enum class region_kind : unsigned char
{
	capture,
	scatter,
	single
};
struct region
{
	::std::size_t end;
	region_kind kind;
};

template <::std::integral Char, typename Output, typename... Args>
struct plan
{
	inline static constexpr ::std::size_t count{sizeof...(Args)};
	inline static constexpr shape shapes[count == 0u ? 1u : count]{source_shape<Char, Output, Args>()...};
	inline static constexpr ::std::size_t reserve_max{[]() consteval {
		::std::size_t result{1u};
		for (auto value : shapes)
		{
			result = d::print_contiguous_char_extent_add_or_unavailable<Char>(result, value.reserve_max);
		}
		return result;
	}()};
	inline static constexpr ::std::size_t context_max{[]() consteval {
		::std::size_t result{32u};
		for (auto value : shapes)
		{
			if (result < value.context_max)
			{
				result = value.context_max;
			}
		}
		if (result < reserve_max)
		{
			result = reserve_max;
		}
		if (result < d::print_stack_buffer_max_size<Char>())
		{
			result = d::print_stack_buffer_max_size<Char>();
		}
		return result;
	}()};
	template <::std::size_t Begin, ::std::size_t End>
	inline static consteval ::std::size_t capture_capacity()
	{
		::std::size_t capacity{32u};
		::std::size_t burst{};
		for (::std::size_t index{Begin}; index != End; ++index)
		{
			auto const item{shapes[index]};
			if (item.capture_resets_burst)
			{
				burst = 0u;
			}
			else
			{
				burst = d::print_contiguous_char_extent_add_or_unavailable<Char>(burst, item.reserve_max);
			}
			if (capacity < burst)
			{
				capacity = burst;
			}
			if (capacity < item.context_max)
			{
				capacity = item.context_max;
			}
			if (capacity < item.dynamic_hint)
			{
				capacity = item.dynamic_hint;
			}
		}
		return capacity;
	}
	template <::std::size_t Begin, ::std::size_t End>
	inline static consteval ::std::size_t scratch_capacity()
	{
		::std::size_t capacity{1u};
		::std::size_t hint{};
		for (::std::size_t index{Begin}; index != End; ++index)
		{
			capacity = d::print_contiguous_char_extent_add_or_unavailable<Char>(capacity, shapes[index].scratch_max);
			hint = d::print_strategy_add_capped(hint, shapes[index].dynamic_hint, d::print_stack_buffer_max_size<Char>());
		}
		return d::print_contiguous_char_extent_add_or_unavailable<Char>(capacity, hint);
	}
	template <::std::size_t Begin>
	inline static consteval region next()
	{
		::std::size_t end{Begin};
		bool context{};
		while (end != count && shapes[end].capture_compatible)
		{
			context |= shapes[end].context;
			++end;
		}
		if (context)
		{
			return {end, region_kind::capture};
		}
		end = Begin;
		while (end != count && shapes[end].scatter_compatible)
		{
			++end;
		}
		if (end != Begin)
		{
			return {end, region_kind::scatter};
		}
		return {Begin + 1u, region_kind::single};
	}
};

template <::std::integral Char, typename Output, typename... Args>
inline consteval bool plan_available()
{
	constexpr bool native_scatter{[]() consteval {
		if constexpr (d::print_uses_byte_scatter_representation<Output>)
		{
			return d::print_has_direct_scatter_write_bytes_operations<Output>;
		}
		else
		{
			return d::print_has_direct_scatter_write_operations<Output>;
		}
	}()};
	if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<Output> ||
				  !native_scatter || !(plan_source_available<Char, Output, Args>() && ...))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t minimum_prefetch_characters{
			(::fast_io::print_scatter_materialize_read_prfch_minimum_payload_bytes + sizeof(Char) - 1u) / sizeof(Char)};
		constexpr bool might_prefetch{
			sizeof...(Args) >= ::fast_io::print_scatter_materialize_read_prfch_minimum_descriptor_count &&
			d::print_scatter_direct_full_output_coalesce_threshold<Char, Output>() >=
				minimum_prefetch_characters * ::fast_io::print_scatter_materialize_read_prfch_minimum_descriptor_count};
		// The original coalescing materializer has a lookahead-prefetch path.
		// Keep records which might enter it on that implementation for now.
		return !might_prefetch && plan<Char, Output, Args...>::reserve_max != SIZE_MAX &&
			   plan<Char, Output, Args...>::context_max != SIZE_MAX &&
			   plan<Char, Output, Args...>::template scratch_capacity<0u, sizeof...(Args)>() != SIZE_MAX;
	}
}

template <typename Element, ::std::size_t Maximum, typename Function>
inline constexpr void storage(::std::size_t size, bool allow_stack, Function &&function)
{
	constexpr ::std::size_t limit{d::print_stack_buffer_max_element_count<Element>()};
	constexpr ::std::size_t capacity{Maximum < limit ? (Maximum == 0u ? 1u : Maximum) : limit};
	if (allow_stack && size <= capacity)
	{
		Element buffer[capacity];
		function(buffer);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<Element> buffer(size == 0u ? 1u : size);
		function(buffer.ptr);
	}
}

template <::std::size_t Begin, typename Graph, typename Function, ::std::size_t... Index>
inline constexpr void visit_range(Graph &graph, ::std::size_t const *selected, description const *descriptions,
								  Function &&function, ::std::index_sequence<Index...>)
{
	auto one = [&]<::std::size_t Position>() constexpr {
		if (descriptions[Position].active)
		{
			visit_selected(graph.template get<Position>(), selected[Position], [&](auto &value) constexpr {
				if constexpr (!::std::same_as<::std::remove_cvref_t<decltype(value)>, ::fast_io::io_null_t>)
				{
					if constexpr (d::print_semantic_top_level_condition_v<decltype(graph.template get<Position>())>)
					{
						using value_type = ::std::remove_cvref_t<decltype(value)>;
						value_type cached{};
						if constexpr (::std::same_as<value_type, ::fast_io::manipulators::chvw_t<typename Graph::char_type>>)
						{
							cached.reference = graph.payloads[Position].character;
						}
						else
						{
							cached.base = graph.payloads[Position].base;
						}
						function(cached, descriptions[Position]);
					}
					else
					{
						function(value, descriptions[Position]);
					}
				}
			});
		}
	};
	(one.template operator()<Begin + Index>(), ...);
}

template <typename Scatter, ::std::integral Char>
inline constexpr void put_scatter(Scatter *&destination, Char const *base, ::std::size_t size)
{
	if constexpr (::std::same_as<Scatter, ::fast_io::io_scatter_t>)
	{
		*destination++ = {base, ::fast_io::details::intrinsics::mul_or_overflow_die(size, sizeof(Char))};
	}
	else
	{
		*destination++ = {base, size};
	}
}

template <::std::integral Char, typename Output, typename T>
inline constexpr void capture_one(Output &out, Char *buffer, Char *&current, Char *end, T &value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::reserve_printable<Char, value_type> ||
				  ::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>)
	{
		::std::size_t const size{[&]() constexpr {
			if constexpr (::fast_io::reserve_printable<Char, value_type>)
			{
				constexpr ::std::size_t size{print_reserve_size(::fast_io::io_reserve_type<Char, value_type>)};
				return size;
			}
			else
			{
				return print_reserve_size(::fast_io::io_reserve_type<Char, value_type>, value);
			}
		}()};
		if (static_cast<::std::size_t>(end - current) < size)
		{
			d::context_capture_flush(out, buffer, current);
		}
		if (size <= static_cast<::std::size_t>(end - buffer))
		{
			current = print_reserve_define(::fast_io::io_reserve_type<Char, value_type>, current, value);
		}
		else
		{
			d::print_control_single<false>(out, value);
		}
	}
	else if constexpr (::fast_io::context_printable_with_static_buffer_size<Char, value_type>)
	{
		using state_type = ::fast_io::details::print_context_state_t<Char, value_type>;
		::fast_io::details::with_print_context_state<state_type>([&](state_type &state) constexpr {
			for (;;)
			{
				if (current == end)
				{
					d::context_capture_flush(out, buffer, current);
				}
				Char *const begin{current};
				auto [next, done] = state.print_context_define(value, begin, end);
				d::validate_context_print_result(begin, end, next, done);
				current = next;
				if (done)
				{
					break;
				}
			}
		});
	}
}

template <::std::size_t Begin, bool Line, typename Plan, typename Output, typename Graph>
inline constexpr void emit_region(Output &out, Graph &graph, ::std::size_t const *selected,
								  description const *descriptions, ::std::size_t remaining)
{
	using Char = typename Output::output_char_type;
	using Scatter = ::std::conditional_t<d::print_uses_byte_scatter_representation<Output>,
										 ::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<Char>>;
	if constexpr (Begin != Plan::count)
	{
		if (remaining == 0u)
		{
			return;
		}
		if (remaining == 1u)
		{
			visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr { d::print_control_single<Line>(out, value); }, ::std::make_index_sequence<Plan::count - Begin>{});
			return;
		}
		constexpr auto region{Plan::template next<Begin>()};
		constexpr auto indices{::std::make_index_sequence<region.end - Begin>{}};
		::std::size_t active{};
		for (::std::size_t index{Begin}; index != region.end; ++index)
		{
			active += descriptions[index].active;
		}
		bool const newline{Line && active == remaining};
		if (active != 0u)
		{
			if constexpr (region.kind == region_kind::single)
			{
				visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr { d::print_control_single<false>(out, value); }, indices);
			}
			else if constexpr (region.kind == region_kind::capture)
			{
				::std::size_t capacity{32u};
				::std::size_t burst{};
				for (::std::size_t index{Begin}; index != region.end; ++index)
				{
					auto const item{descriptions[index]};
					if (!item.active)
					{
						continue;
					}
					if (item.capture == capture_kind::reserve)
					{
						burst += item.reserve_size;
						if (capacity < burst)
						{
							capacity = burst;
						}
					}
					else
					{
						burst = 0u;
						if (capacity < item.context_size)
						{
							capacity = item.context_size;
						}
						if (capacity < item.dynamic_hint)
						{
							capacity = item.dynamic_hint;
						}
					}
				}
				storage<Char, Plan::template capture_capacity<Begin, region.end>()>(capacity, true, [&](Char *buffer) constexpr {
					Char *current{buffer};
					Char *const end{buffer + capacity};
					visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr { capture_one(out, buffer, current, end, value); }, indices);
					if (newline)
					{
						if (current == end)
						{
							d::context_capture_flush(out, buffer, current);
						}
						*current++ = ::fast_io::char_literal_v<u8'\n', Char>;
					}
					d::context_capture_flush(out, buffer, current);
				});
			}
			else
			{
				bool has_scatter{};
				bool has_reserve{};
				bool has_dynamic{};
				bool preserve_static{};
				::std::size_t reserve_size{static_cast<::std::size_t>(newline)};
				::std::size_t dynamic_hint{};
				for (::std::size_t index{Begin}; index != region.end; ++index)
				{
					auto const item{descriptions[index]};
					if (!item.active)
					{
						continue;
					}
					has_scatter |= item.scatter == scatter_kind::scatter;
					has_reserve |= item.scatter == scatter_kind::reserve;
					has_dynamic |= item.scatter == scatter_kind::dynamic;
					preserve_static |= item.static_fragment;
					if (item.scatter == scatter_kind::reserve)
					{
						reserve_size += item.reserve_size;
					}
					dynamic_hint = d::print_strategy_add_capped(dynamic_hint, item.dynamic_hint, d::print_stack_buffer_max_size<Char>());
				}
				::std::size_t dynamic_size{};
				if (has_dynamic)
				{
					visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description item) constexpr {
						using value_type = ::std::remove_cvref_t<decltype(value)>;
						if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>)
						{
							if (item.scatter == scatter_kind::dynamic)
							{
								dynamic_size = d::print_contiguous_char_extent_add_or_unavailable<Char>(dynamic_size,
									print_reserve_size(::fast_io::io_reserve_type<Char, value_type>, value));
							}
						} }, indices);
				}
				::std::size_t const total{d::print_contiguous_char_extent_add_or_unavailable<Char>(reserve_size, dynamic_size)};
				if (total == SIZE_MAX)
				{
					visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr {
						if (--remaining == 0u) { d::print_control_single<Line>(out, value); }
						else { d::print_control_single<false>(out, value); } }, ::std::make_index_sequence<Plan::count - Begin>{});
					return;
				}
				if (has_scatter && !has_reserve && !has_dynamic)
				{
					constexpr ::std::size_t threshold{d::print_scatter_direct_full_output_coalesce_threshold<Char, Output>()};
					bool materialized{};
					if constexpr (threshold != 0u && d::print_has_direct_write_operations<Output>)
					{
						if (!preserve_static && active <= d::print_scatter_pre_descriptor_materialization_max_descriptor_count)
						{
							::std::size_t size{};
							visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr {
								using value_type = ::std::remove_cvref_t<decltype(value)>;
								if constexpr (d::retained_scatter_printable_v<Char, decltype((value))> ||
									d::print_static_scatter_traits<Char, value_type>::available)
								{
									auto scatter{d::print_native_scatter_define<Char>(value)};
									size = d::print_contiguous_char_extent_add_or_unavailable<Char>(size, scatter.len);
								} }, indices);
							size = d::print_contiguous_char_extent_add_or_unavailable<Char>(size, static_cast<::std::size_t>(newline));
							if (size != SIZE_MAX && size <= threshold)
							{
								materialized = true;
								if (size != 0u)
								{
									storage<Char, threshold>(size, threshold <= d::print_stack_buffer_max_size<Char>(), [&](Char *buffer) constexpr {
										Char *current{buffer};
										visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr {
											using value_type = ::std::remove_cvref_t<decltype(value)>;
											if constexpr (d::retained_scatter_printable_v<Char, decltype((value))> ||
												d::print_static_scatter_traits<Char, value_type>::available)
											{
														current = d::print_n_scatter_materialize<1u, Char>(current, value);
											} }, indices);
										if (newline)
										{
											*current++ = ::fast_io::char_literal_v<u8'\n', Char>;
										}
										::fast_io::operations::decay::write_all_decay_dispatch(out, buffer, current);
									});
								}
							}
						}
					}
					if (!materialized)
					{
						storage<Scatter, Plan::count + 1u>(active + static_cast<::std::size_t>(newline), true, [&](Scatter *scatters) constexpr {
							Scatter *current{scatters};
							visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description) constexpr {
								using value_type = ::std::remove_cvref_t<decltype(value)>;
								if constexpr (d::retained_scatter_printable_v<Char, decltype((value))> ||
									d::print_static_scatter_traits<Char, value_type>::available)
								{
									auto scatter{d::print_native_scatter_define<Char>(value)};
									put_scatter(current, scatter.base, scatter.len);
								} }, indices);
							if (newline)
							{
								*current++ = d::line_scatter_common<Char, ::std::conditional_t<::std::same_as<Scatter, ::fast_io::io_scatter_t>, void, Char>>;
							}
							auto const count{static_cast<::std::size_t>(current - scatters)};
							if (preserve_static)
							{
								d::print_scatter_write_all_preserving_static_fragments(out, scatters, count);
							}
							else
							{
								d::print_scatter_write_all_dispatch(out, scatters, count);
							}
						});
					}
				}
				else
				{
					::std::size_t const stack_budget{d::print_contiguous_char_extent_add_or_unavailable<Char>(reserve_size, dynamic_hint)};
					bool const allow_stack{!has_dynamic || (stack_budget <= d::print_stack_buffer_max_size<Char>() && total <= stack_budget)};
					// Descriptor allocation precedes scratch allocation, as in the
					// ordinary mixed controller. Reserve-only runs need no descriptors.
					auto materialize = [&](Scatter *scatters) constexpr {
						storage<Char, Plan::template scratch_capacity<Begin, region.end>()>(total, allow_stack, [&](Char *buffer) constexpr {
							Char *current{buffer};
							Char *reserve_begin{buffer};
							Scatter *scatter_current{scatters};
							bool reserve_open{};
							auto close_reserve = [&]() constexpr {
								if (reserve_open)
								{
									put_scatter(scatter_current, reserve_begin, static_cast<::std::size_t>(current - reserve_begin));
									reserve_open = false;
								}
							};
							visit_range<Begin>(graph, selected, descriptions, [&](auto &value, description item) constexpr {
								using value_type = ::std::remove_cvref_t<decltype(value)>;
								if constexpr (::fast_io::reserve_printable<Char, value_type> ||
									::fast_io::dynamic_reserve_with_possible_static_stack_size<Char, value_type>)
								{
									if (item.scatter != scatter_kind::scatter)
									{
										if (!reserve_open) { reserve_open = true; reserve_begin = current; }
										current = print_reserve_define(::fast_io::io_reserve_type<Char, value_type>, current, value);
										return;
									}
								}
								if constexpr (d::retained_scatter_printable_v<Char, decltype((value))> ||
									d::print_static_scatter_traits<Char, value_type>::available)
								{
									close_reserve();
									auto scatter{d::print_native_scatter_define<Char>(value)};
									put_scatter(scatter_current, scatter.base, scatter.len);
								} }, indices);
							if (!has_scatter)
							{
								if (newline)
								{
									*current++ = ::fast_io::char_literal_v<u8'\n', Char>;
								}
								::fast_io::operations::decay::write_all_decay_dispatch(out, buffer, current);
							}
							else
							{
								if (newline && reserve_open)
								{
									*current++ = ::fast_io::char_literal_v<u8'\n', Char>;
								}
								else if (newline)
								{
									*scatter_current++ = d::line_scatter_common<Char, ::std::conditional_t<::std::same_as<Scatter, ::fast_io::io_scatter_t>, void, Char>>;
								}
								close_reserve();
								auto const count{static_cast<::std::size_t>(scatter_current - scatters)};
								if (preserve_static)
								{
									d::print_scatter_write_all_preserving_static_fragments(out, scatters, count);
								}
								else
								{
									d::print_scatter_write_all_dispatch(out, scatters, count);
								}
							}
						});
					};
					if (has_scatter)
					{
						storage<Scatter, Plan::count + 1u>(active + static_cast<::std::size_t>(newline), true, materialize);
					}
					else
					{
						materialize(nullptr);
					}
				}
			}
		}
		emit_region<region.end, Line, Plan>(out, graph, selected, descriptions, remaining - active);
	}
}

template <bool Line, typename Cache, typename Output, typename... Args>
inline constexpr void emit_general_cached(Output &out, Cache *cached, Args &...args)
{
	using Char = typename Output::output_char_type;
	static_assert(plan_available<Char, Output, Args...>());
	using Plan = plan<Char, Output, Args...>;
	source_graph<Char, ::std::index_sequence_for<Args...>, Args...> graph{args...};
	::std::size_t selected[sizeof...(Args) == 0u ? 1u : sizeof...(Args)]{};
	description descriptions[sizeof...(Args) == 0u ? 1u : sizeof...(Args)]{};
	::std::size_t active{};
	[&]<::std::size_t... Index>(::std::index_sequence<Index...>) constexpr {
		auto initialize = [&]<::std::size_t Position>() constexpr {
			auto &source{graph.template get<Position>()};
			if constexpr (!::std::same_as<Cache, void>)
			{
				using Slot = typename Cache::template slot_type<Position>;
				if constexpr (Slot::optional)
				{
					auto &slot{cached->template slot<Position>()};
					using value_type = typename Slot::leaf_type;
					selected[Position] = slot.enabled() ? (Slot::traits::first_null ? 1u : 0u) : (Slot::traits::first_null ? 0u : 1u);
					descriptions[Position] = slot.enabled() ? leaf_description<Char, Output, value_type>() : leaf_description<Char, Output, ::fast_io::io_null_t>();
					if (slot.enabled())
					{
						if constexpr (::std::same_as<value_type, ::fast_io::manipulators::chvw_t<Char>>)
						{
							graph.payloads[Position].character = slot.get().reference;
						}
						else
						{
							graph.payloads[Position].base = slot.get().base;
						}
					}
				}
				else
				{
					descriptions[Position] = leaf_description<Char, Output, decltype(source)>();
				}
			}
			else
			{
				selected[Position] = select(source);
				visit_selected(source, selected[Position], [&](auto &value) constexpr {
					descriptions[Position] = leaf_description<Char, Output, decltype(value)>();
					using value_type = ::std::remove_cvref_t<decltype(value)>;
					if constexpr (d::print_semantic_top_level_condition_v<decltype(source)> &&
								  !::std::same_as<value_type, ::fast_io::io_null_t>)
					{
						decltype(auto) forwarded{d::print_semantic_input_forward<Char>(value)};
						static_assert(::std::same_as<decltype(forwarded), value_type>);
						if constexpr (::std::same_as<value_type, ::fast_io::manipulators::chvw_t<Char>>)
						{
							graph.payloads[Position].character = forwarded.reference;
						}
						else
						{
							graph.payloads[Position].base = forwarded.base;
						}
					}
				});
			}
			active += descriptions[Position].active;
		};
		(initialize.template operator()<Index>(), ...);
	}(::std::index_sequence_for<Args...>{});
	if (active == 0u)
	{
		if constexpr (Line)
		{
			::fast_io::operations::decay::char_put_decay_dispatch(out, ::fast_io::char_literal_v<u8'\n', Char>);
		}
		return;
	}
	emit_region<0u, Line, Plan>(out, graph, selected, descriptions, active);
}

template <bool Line, typename Output, typename... Args>
inline constexpr void emit_general(Output &out, Args &...args)
{
	emit_general_cached<Line>(out, static_cast<void *>(nullptr), args...);
}

template <::std::integral Char, typename T, bool Condition = d::print_semantic_top_level_condition_v<T &>>
struct simple_condition
{
	inline static constexpr bool condition{};
	inline static constexpr bool available{true};
	using leaf_type = T;
};

template <::std::integral Char, typename T>
struct simple_condition<Char, T, true>
{
	using node_type = ::std::remove_reference_t<decltype(d::print_semantic_node_ref(::std::declval<T &>()))>;
	using first_type = ::std::remove_cvref_t<decltype(::std::declval<node_type &>().t1)>;
	using second_type = ::std::remove_cvref_t<decltype(::std::declval<node_type &>().t2)>;
	inline static constexpr bool first_null{::std::same_as<first_type, ::fast_io::io_null_t>};
	inline static constexpr bool second_null{::std::same_as<second_type, ::fast_io::io_null_t>};
	inline static constexpr bool condition{true};
	inline static constexpr bool available{first_null != second_null && optional_source_available<Char, T>() &&
										   !d::print_semantic_top_level_condition_v<first_type> && !d::print_semantic_top_level_condition_v<second_type>};
	using leaf_type = ::std::conditional_t<first_null, second_type, first_type>;
};

template <::std::size_t Index, ::std::integral Char, typename T, bool Optional = simple_condition<Char, T>::condition>
struct compact_slot
{
	inline static constexpr bool optional{};
	using leaf_type = T;
	T *source;
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr explicit compact_slot(T &value) noexcept : source(__builtin_addressof(value))
	{}
	inline static constexpr bool enabled() noexcept
	{
		return !::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;
	}
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr T &get() const noexcept
	{
		return *source;
	}
};

template <::std::size_t Index, ::std::integral Char, typename T>
struct compact_slot<Index, Char, T, true>
{
	using traits = simple_condition<Char, T>;
	using leaf_type = typename traits::leaf_type;
	inline static constexpr bool optional{true};
	leaf_type value{};
	bool active{};
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr explicit compact_slot(T &source)
	{
		auto &&node{d::print_semantic_node_ref(source)};
		active = traits::first_null ? !node.pred : node.pred;
		if (active)
		{
			if constexpr (traits::first_null)
			{
				value = d::print_semantic_input_forward<Char>(node.t2);
			}
			else
			{
				value = d::print_semantic_input_forward<Char>(node.t1);
			}
		}
	}
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool enabled() const noexcept
	{
		return active;
	}
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr leaf_type &get() noexcept
	{
		return value;
	}
};

template <::std::integral Char, typename Sequence, typename... Args>
struct compact_graph;

template <::std::integral Char, ::std::size_t... Index, typename... Args>
struct compact_graph<Char, ::std::index_sequence<Index...>, Args...> : compact_slot<Index, Char, Args>...
{
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr explicit compact_graph(Args &...args) : compact_slot<Index, Char, Args>(args)...
	{}
	template <::std::size_t Position>
	using source_type =
#if __cplusplus > 202302L && __cpp_pack_indexing >= 202311L
		Args...[Position];
#elif FAST_IO_HAS_BUILTIN(__type_pack_element)
		__type_pack_element<Position, Args...>;
#else
		typename fallback_type_at<Position, Args...>::type;
#endif
	template <::std::size_t Position>
	using slot_type = compact_slot<Position, Char, source_type<Position>>;
	// GCC 16 cannot mangle a native pack-index expression in this return type. Deducing the same reference avoids it.
	template <::std::size_t Position>
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto) slot() noexcept
	{
		return static_cast<slot_type<Position> &>(*this);
	}
};

template <::std::integral Char, typename Output, typename... Args>
inline consteval bool compact_available()
{
	if constexpr (!(simple_condition<Char, Args>::available && ...) || !plan_available<Char, Output, Args...>())
	{
		return false;
	}
	else if constexpr (d::print_scatter_direct_full_output_coalesce_threshold<Char, Output>() != 0u)
	{
		return false;
	}
	else
	{
		using Plan = plan<Char, Output, Args...>;
		constexpr description leaves[sizeof...(Args) == 0u ? 1u : sizeof...(Args)]{
			leaf_description<Char, Output, typename simple_condition<Char, Args>::leaf_type>()...};
		constexpr bool optional[sizeof...(Args) == 0u ? 1u : sizeof...(Args)]{simple_condition<Char, Args>::condition...};
		::std::size_t last_optional_end{};
		for (::std::size_t index{}; index != Plan::count; ++index)
		{
			if (optional[index])
			{
				last_optional_end = index + 1u;
			}
		}
		::std::size_t begin{};
		while (begin != Plan::count)
		{
			// Only an already completed region establishes a handoff boundary.
			// A context or mixed scatter run may include the final condition and
			// its fixed tail, so never split a run at last_optional_end itself.
			if (last_optional_end <= begin)
			{
				return true;
			}
			::std::size_t end{begin};
			bool context{};
			while (end != Plan::count && Plan::shapes[end].capture_compatible)
			{
				context |= Plan::shapes[end].context;
				++end;
			}
			if (context)
			{
				return false;
			}
			end = begin;
			while (end != Plan::count && Plan::shapes[end].scatter_compatible)
			{
				++end;
			}
			if (end == begin)
			{
				++begin;
				continue;
			}
			bool pure_scatter{true};
			for (::std::size_t index{begin}; index != end; ++index)
			{
				pure_scatter &= !leaves[index].active || leaves[index].scatter == scatter_kind::scatter;
			}
			// A final source can only contribute zero or one active leaf, so it
			// retains the original single-control path regardless of its protocol.
			if (!pure_scatter && !(end == Plan::count && end - begin == 1u))
			{
				return false;
			}
			begin = end;
		}
		return true;
	}
}

enum class compact_selection : unsigned char
{
	selected,
	all_off,
	all_on
};

template <::std::size_t Begin, typename Graph, ::std::size_t... Index>
inline consteval ::std::size_t compact_minimum(::std::index_sequence<Index...>)
{
	return (0u + ... + static_cast<::std::size_t>(!Graph::template slot_type<Begin + Index>::optional && !::std::same_as<::std::remove_cvref_t<typename Graph::template slot_type<Begin + Index>::leaf_type>, ::fast_io::io_null_t>));
}

template <::std::size_t Begin, typename Graph, ::std::size_t... Index>
inline consteval ::std::size_t compact_maximum(::std::index_sequence<Index...>)
{
	return (0u + ... + static_cast<::std::size_t>(!::std::same_as<::std::remove_cvref_t<typename Graph::template slot_type<Begin + Index>::leaf_type>, ::fast_io::io_null_t>));
}

template <::std::size_t Begin, typename Graph, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t compact_count(Graph &graph, ::std::index_sequence<Index...>)
{
	return (0u + ... + static_cast<::std::size_t>(graph.template slot<Begin + Index>().enabled()));
}

template <::std::size_t Begin, compact_selection Selection, typename Graph, typename Function, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_visit(Graph &graph, Function &&function, ::std::index_sequence<Index...>)
{
	auto one = [&]<::std::size_t Position>() constexpr {
		using Slot = typename Graph::template slot_type<Position>;
		if constexpr (!::std::same_as<::std::remove_cvref_t<typename Slot::leaf_type>, ::fast_io::io_null_t>)
		{
			auto &slot{graph.template slot<Position>()};
			if constexpr (!Slot::optional || Selection == compact_selection::all_on)
			{
				function(slot.get());
			}
			else if constexpr (Selection == compact_selection::selected)
			{
				if (slot.enabled())
				{
					function(slot.get());
				}
			}
		}
	};
	(one.template operator()<Begin + Index>(), ...);
}

template <::std::size_t Begin, compact_selection Selection, bool Newline, typename Output, typename Graph, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_scatter(Output &out, Graph &graph, ::std::index_sequence<Index...> indices)
{
	using Char = typename Output::output_char_type;
	using Scatter = ::std::conditional_t<d::print_uses_byte_scatter_representation<Output>, ::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<Char>>;
	constexpr ::std::size_t minimum{compact_minimum<Begin, Graph>(indices)};
	constexpr ::std::size_t maximum{compact_maximum<Begin, Graph>(indices)};
	constexpr ::std::size_t capacity{maximum + static_cast<::std::size_t>(Newline)};
	auto materialize = [&](Scatter *scatters) constexpr {
		Scatter *current{scatters};
		compact_visit<Begin, Selection>(graph, [&](auto &value) constexpr {
			auto scatter{d::print_native_scatter_define<Char>(value)};
			put_scatter(current, scatter.base, scatter.len); }, indices);
		if constexpr (Newline)
		{
			*current++ = d::line_scatter_common<Char, ::std::conditional_t<::std::same_as<Scatter, ::fast_io::io_scatter_t>, void, Char>>;
		}
		if constexpr (Selection == compact_selection::all_off)
		{
			d::print_scatter_write_all_dispatch(out, scatters, minimum + static_cast<::std::size_t>(Newline));
		}
		else if constexpr (Selection == compact_selection::all_on)
		{
			d::print_scatter_write_all_dispatch(out, scatters, maximum + static_cast<::std::size_t>(Newline));
		}
		else
		{
			d::print_scatter_write_all_dispatch(out, scatters, static_cast<::std::size_t>(current - scatters));
		}
	};
	if constexpr (d::print_stack_buffer_size_within_limit<capacity, Scatter>)
	{
		// Every selected shape uses stack descriptors here. Exposing a fixed
		// count for the common all-off/all-on arms restores ordinary unrolling.
		Scatter scatters[capacity == 0u ? 1u : capacity];
		materialize(scatters);
	}
	else
	{
		::std::size_t const count{Selection == compact_selection::all_off ? minimum : Selection == compact_selection::all_on ? maximum
																															 : compact_count<Begin>(graph, indices)};
		storage<Scatter, capacity>(count + static_cast<::std::size_t>(Newline), true, materialize);
	}
}

template <::std::size_t Begin, bool Newline, typename Output, typename Graph, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_scatter_select(Output &out, Graph &graph, ::std::index_sequence<Index...> indices)
{
	constexpr ::std::size_t minimum{compact_minimum<Begin, Graph>(indices)};
	constexpr ::std::size_t maximum{compact_maximum<Begin, Graph>(indices)};
	if constexpr (minimum == maximum)
	{
		compact_scatter<Begin, compact_selection::all_on, Newline>(out, graph, indices);
	}
	else
	{
		bool const any{(false || ... || (Graph::template slot_type<Begin + Index>::optional && graph.template slot<Begin + Index>().enabled()))};
		if (!any)
		{
			if constexpr (minimum != 0u || Newline)
			{
				compact_scatter<Begin, compact_selection::all_off, Newline>(out, graph, indices);
			}
		}
		else
		{
			bool const all{(true && ... && (!Graph::template slot_type<Begin + Index>::optional || graph.template slot<Begin + Index>().enabled()))};
			if (all)
			{
				compact_scatter<Begin, compact_selection::all_on, Newline>(out, graph, indices);
			}
			else
			{
				compact_scatter<Begin, compact_selection::selected, Newline>(out, graph, indices);
			}
		}
	}
}

template <::std::size_t Begin, compact_selection Selection, bool Line, typename Output, typename Graph, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_project_controls(Output &out, Graph &graph, ::std::index_sequence<Index...> indices);

template <::std::size_t Begin, bool Line, typename Plan, typename Output, typename Graph>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_regions(Output &out, Graph &graph)
{
	if constexpr (Begin != Plan::count)
	{
		constexpr auto suffix{::std::make_index_sequence<Plan::count - Begin>{}};
		if constexpr (compact_minimum<Begin, Graph>(suffix) == compact_maximum<Begin, Graph>(suffix))
		{
			// Begin is either the record start or the preceding Plan::next end.
			// The whole fixed suffix now has exactly the original argument pack;
			// hand off before querying any of its mandatory producers. An empty
			// suffix already left newline ownership with the preceding region.
			if constexpr (compact_minimum<Begin, Graph>(suffix) != 0u)
			{
				compact_project_controls<Begin, compact_selection::all_on, Line>(out, graph, suffix);
			}
		}
		else
		{
			if constexpr (compact_minimum<Begin, Graph>(suffix) < 2u)
			{
				::std::size_t const active{compact_count<Begin>(graph, suffix)};
				if (active == 0u)
				{
					return;
				}
				if (active == 1u)
				{
					compact_visit<Begin, compact_selection::selected>(graph, [&](auto &value) constexpr { d::print_control_single<Line>(out, value); }, suffix);
					return;
				}
			}
			constexpr auto region{Plan::template next<Begin>()};
			constexpr auto indices{::std::make_index_sequence<region.end - Begin>{}};
			if constexpr (region.kind == region_kind::single ||
						  (region.end == Plan::count && region.end - Begin == 1u))
			{
				compact_visit<Begin, compact_selection::selected>(graph, [&](auto &value) constexpr { d::print_control_single<false>(out, value); }, indices);
			}
			else
			{
				constexpr auto tail{::std::make_index_sequence<Plan::count - region.end>{}};
				if constexpr (!Line || compact_minimum<region.end, Graph>(tail) != 0u)
				{
					compact_scatter_select<Begin, false>(out, graph, indices);
				}
				else if constexpr (compact_maximum<region.end, Graph>(tail) == 0u)
				{
					compact_scatter_select<Begin, true>(out, graph, indices);
				}
				else
				{
					if (compact_count<region.end>(graph, tail) == 0u)
					{
						compact_scatter_select<Begin, true>(out, graph, indices);
					}
					else
					{
						compact_scatter_select<Begin, false>(out, graph, indices);
					}
				}
			}
			compact_regions<region.end, Line, Plan>(out, graph);
		}
	}
}

template <::std::size_t Begin, compact_selection Selection, bool Line, typename Output, typename Graph, ::std::size_t... Index>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compact_project_controls(Output &out, Graph &graph, ::std::index_sequence<Index...> indices)
{
	static_assert(Selection != compact_selection::selected);
	constexpr ::std::size_t count{Selection == compact_selection::all_off ? compact_minimum<Begin, Graph>(indices) : compact_maximum<Begin, Graph>(indices)};
	if constexpr (count == 0u)
	{
		if constexpr (Line)
		{
			using Char = typename Output::output_char_type;
			::fast_io::operations::decay::char_put_decay_dispatch(out, ::fast_io::char_literal_v<u8'\n', Char>);
		}
	}
	else
	{
		constexpr auto projection{[]() consteval {
			struct positions
			{
				::std::size_t values[sizeof...(Index)];
			};
			positions result{};
			::std::size_t current{};
			auto one = [&]<::std::size_t Position>() consteval {
				using Slot = typename Graph::template slot_type<Position>;
				if constexpr ((!Slot::optional || Selection == compact_selection::all_on) &&
							  !::std::same_as<::std::remove_cvref_t<typename Slot::leaf_type>, ::fast_io::io_null_t>)
				{
					result.values[current++] = Position;
				}
			};
			(one.template operator()<Begin + Index>(), ...);
			return result;
		}()};
		[&]<::std::size_t... Position>(::std::index_sequence<Position...>) constexpr {
			// Complete-record and fixed-suffix projections reuse the ordinary
			// scanners, context windows, allocation boundaries and write CPOs. Only the
			// caller performs whole-record status lookup; a projection must not
			// re-enter semantic normalization or status dispatch.
			d::print_controls_impl<Line, Output, 0u>(out, graph.template slot<projection.values[Position]>().get()...);
		}(::std::make_index_sequence<count>{});
	}
}

template <bool Line, typename Output, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void emit(Output &out, Args &...args)
{
	using Char = typename Output::output_char_type;
	if constexpr ((simple_condition<Char, Args>::available && ...) && plan_available<Char, Output, Args...>())
	{
		using Plan = plan<Char, Output, Args...>;
		using Graph = compact_graph<Char, ::std::index_sequence_for<Args...>, Args...>;
		constexpr auto indices{::std::index_sequence_for<Args...>{}};
		// Snapshot every selected ABI-small condition value before any producer
		// query. Mandatory sources remain references throughout both projections.
		Graph graph{args...};
		if constexpr (compact_minimum<0u, Graph>(indices) == compact_maximum<0u, Graph>(indices))
		{
			compact_project_controls<0u, compact_selection::all_on, Line>(out, graph, indices);
		}
		else
		{
			auto const any{[&]<::std::size_t... Index>(::std::index_sequence<Index...>) constexpr {
				return (false || ... || (Graph::template slot_type<Index>::optional && graph.template slot<Index>().enabled()));
			}(indices)};
			if (!any)
			{
				compact_project_controls<0u, compact_selection::all_off, Line>(out, graph, indices);
				return;
			}
			auto const all{[&]<::std::size_t... Index>(::std::index_sequence<Index...>) constexpr {
				return (true && ... && (!Graph::template slot_type<Index>::optional || graph.template slot<Index>().enabled()));
			}(indices)};
			if (all)
			{
				compact_project_controls<0u, compact_selection::all_on, Line>(out, graph, indices);
				return;
			}
			if constexpr (compact_available<Char, Output, Args...>())
			{
				compact_regions<0u, Line, Plan>(out, graph);
			}
			else
			{
				// Reuse the exact forwarded values and selections. Even the shared
				// mixed path must not repeat aliasing/forwarding CPOs or reread a
				// condition whose value another selected forwarder could change.
				emit_general_cached<Line>(out, __builtin_addressof(graph), args...);
			}
		}
	}
	else
	{
		emit_general<Line>(out, args...);
	}
}

} // namespace print_semantic_linear

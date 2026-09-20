#pragma once

/*
 * General concat sizing and materialization pipeline (IO operation core).
 *
 * Public concat calls arrive with a destination strlike type and an ordered
 * source record. This file applies `io_print_alias`/forward exactly once,
 * expands semantic `pack`/`cond`/`io_null`/`width` structure into the active
 * record, and evaluates the compiler-constant gate while the required source
 * evidence is still visible. It then selects direct destination construction,
 * append/grow, generic buffering, or staging and chooses reserve, dynamic
 * reserve, scatter, reserve-scatter, context, or direct printable leaves.
 *
 * Concat therefore belongs to the IO level, not the FMT level: it accepts
 * already-typed components from format lowering as well as ordinary non-format
 * arguments. It shares object CPOs with print but terminates in strlike
 * allocation/commit protocols instead of stream write CPOs.
 *
 * At the value-semantics level, constructing a fresh string with concat may
 * resemble formatting into a whole-input scanner, for example
 * `to<decltype(mnp::whole_get(str))>(args...)`. That spelling is not a usable
 * substitute. `whole_get(str)` is a scanner carrier bound to the existing
 * object `str`; its type contains that borrowed reference, whereas `to<T>` is a
 * value-producing operation which must first construct a new `T` and then scan
 * into it. Naming only the carrier's type loses the bound object and normally
 * leaves `T` non-default-constructible. Use concat when the operation must
 * create and return a new string-like value. For an explicitly supplied
 * existing destination, `inplace_to(mnp::whole_get(str), args...)` is the
 * corresponding print-to-whole-scan composition. This semantic relationship
 * does not imply a shared implementation: concat owns direct strlike
 * materialization, while inplace_to remains a print/scan pipeline.
 */

#include "../operations/printimpl/scatter_copy.h"

namespace fast_io::details::decay
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t calculate_scatter_reserve_size_unit()
{
	using real_type = ::std::remove_cvref_t<T>;
	if constexpr (reserve_printable<char_type, real_type>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, real_type>)};
		return sz;
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_reserve_size()
{
	if constexpr (sizeof...(Args) == 0)
	{
		return calculate_scatter_reserve_size_unit<char_type, T>();
	}
	else
	{
		// This historical exact-sum helper is also consumed by non-concat code whose caller owns overflow handling.
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			calculate_scatter_reserve_size_unit<char_type, T>(), calculate_scatter_reserve_size<char_type, Args...>());
	}
}

/// @brief Computes concat's optional static reserve aggregate, or reports that the one-buffer plan is unavailable.
/// @details Unlike the shared exact-sum helper above, concat can preserve valid component protocols when conservative
///          upper bounds do not fit one C++ pointer domain. Keeping this classifier concat-specific prevents unrelated
///          consumers such as `to` and kernel printing from interpreting SIZE_MAX as an allocation request.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_concat_scatter_reserve_size_or_unavailable() noexcept
{
	::std::size_t const current{::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		0u, calculate_scatter_reserve_size_unit<char_type, T>())};
	if constexpr (sizeof...(Args) == 0)
	{
		return current;
	}
	else
	{
		return ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
			current, calculate_concat_scatter_reserve_size_or_unavailable<char_type, Args...>());
	}
}

/// @brief Folds an already-selected reserve leaf run into one contiguous concat destination.
/// @details The recursive `%.*f` condition-record deletion matrix proves this local fold jointly necessary with the
///          materialized builder and active-record selector on Clang 21--23. Deleting only this edge makes a successful
///          constant root reach the native precision formatter again. Unknown condition arms never enter this helper,
///          and the complete candidate fails while growing text on Clang 16--20, so no earlier Clang inherits it.
template <typename T>
struct concat_float_scalar_traits
{
	inline static constexpr bool available{};
};

template <::fast_io::manipulators::scalar_flags flags, typename T>
struct concat_float_scalar_traits<::fast_io::manipulators::scalar_manip_t<flags, T>>
{
	inline static constexpr bool available{::fast_io::details::my_floating_point<T>};
};

/// Keep the runtime floating reserve formatter's call boundary private to concat. The ordinary print CPO remains
/// cost-model controlled; this small wrapper prevents GCC from cloning the large formatter into each mixed concat
/// chain, while the constant-valued specialization below retains its separate fold-friendly policy.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
#if defined(__GNUC__) && !defined(__clang__)
[[gnu::noinline]]
#endif
inline constexpr char_type *concat_float_reserve_emit(
	char_type *out, ::fast_io::manipulators::scalar_manip_t<flags, T> &value) noexcept
{
	return print_reserve_define(
		::fast_io::io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, out, value);
}

/// A caller-visible constant floating scalar needs the stronger flattening boundary for GCC to fold the complete
/// spelling. Runtime scalars retain the smaller wrapper above: flattening their complete DA formatter enlarges the
/// hot loop and was slower in isolated float-only concat records.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
FAST_IO_GNU_ALWAYS_INLINE
#if defined(__GNUC__) && !defined(__clang__)
	[[gnu::flatten]]
#endif
	inline constexpr char_type *concat_constant_float_reserve_emit(
		char_type *out, ::fast_io::manipulators::scalar_manip_t<flags, T> &value) noexcept
{
	return print_reserve_define(
		::fast_io::io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, out, value);
}

template <bool line, ::std::integral char_type, typename T, typename... Args>
#if defined(__clang__) && 21 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#elif defined(__GNUC__) && !defined(__clang__)
// GCC does not infer the recursive reserve chain's inline boundary after the
// arguments were changed to references (the old by-value chain was fully
// folded).  Keep this force-inline contract local to concat; the generic print
// reserve CPO remains cost-model controlled for all other destinations.
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr char_type *print_reserve_define_chain_impl(char_type *p, T &t, Args &...args)
{
	// The default integer scalar carrier has a stronger local spelling proof than
	// arbitrary reserve providers. Keep its force-inlined decimal leaf confined to
	// concat; the global integer reserve CPO retains its normal cost model.
	auto emit_one = [](char_type *out, auto &value) constexpr {
		using value_type = ::std::remove_cvref_t<decltype(value)>;
		if constexpr (
#if defined(__GNUC__) && !defined(__clang__)
			::fast_io::details::decay::print_fixed_hot_scalar_traits<value_type>::available
#else
			false
#endif
		)
		{
			return ::fast_io::details::decay::print_fixed_static_reserve_emit_one<char_type>(out, value);
		}
		else if constexpr (
#if defined(__GNUC__) && !defined(__clang__)
			::fast_io::details::decay::concat_float_scalar_traits<value_type>::available
#else
			false
#endif
		)
		{
			FAST_IO_IF_NOT_CONSTEVAL
			{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
				if (__builtin_constant_p(value.reference))
				{
					return ::fast_io::details::decay::concat_constant_float_reserve_emit<char_type>(out, value);
				}
#endif
			}
			return ::fast_io::details::decay::concat_float_reserve_emit<char_type>(out, value);
		}
		else
		{
			return print_reserve_define(io_reserve_type<char_type, value_type>, out, value);
		}
	};
	if constexpr (sizeof...(Args) == 0)
	{
		p = emit_one(p, t);
		if constexpr (line)
		{
			*p = char_literal_v<u8'\n', char_type>;
			++p;
		}
		return p;
	}
	else
	{
		return print_reserve_define_chain_impl<line>(
			emit_one(p, t), args...);
	}
}

/// @brief Stores the one-shot metadata selected for a mixed concat component.
/// @details Component kind remains a compile-time property of the corresponding argument type, so no run-time tag is
///          needed. Reserve leaves store their bound in `scatter.len` and leave `base` unused; retained scatter leaves
///          use both descriptor fields. This two-word tagged-by-type representation avoids one redundant machine word
///          per leaf while giving the planning pass stable storage without replaying an object-dependent customization.
template <::std::integral char_type>
struct concat_mixed_component_cache
{
	::fast_io::basic_io_scatter_t<char_type> scatter{};
};

/// @brief Proves that an optional concat planning table fits the bounded hot stack budget.
/// @details The configured print limit is a safety ceiling; four KiB is a separate code-generation and page-pressure
///          ceiling for metadata that does not itself hold formatted output. Division before multiplication makes the
///          test valid even for an adversarial template argument count. Plans above the limit keep identical semantics
///          in dynamic storage rather than growing the caller's frame with the flattened pack length.
template <::std::size_t count, typename element_type>
inline consteval bool concat_metadata_stack_safe() noexcept
{
	constexpr ::std::size_t configured_budget{
		::fast_io::details::decay::print_stack_buffer_max_bytes()};
	constexpr ::std::size_t preferred_budget{4u * 1024u};
	constexpr ::std::size_t budget{
		configured_budget < preferred_budget ? configured_budget : preferred_budget};
	return budget != 0u && count <= budget / sizeof(element_type);
}

/// @brief Measures every mixed concat component exactly once and caches the selected representation.
/// @details Protocol priority is identical to emission: static reserve, then dynamic reserve, then retained scatter.
///          The all-retained-scatter strategy is selected before this helper, so a dual-protocol run can still prefer
///          its exact descriptors globally. Each individual extent is checked before it can participate in pointer
///          arithmetic. Aggregate overflow returns SIZE_MAX but does not stop measurement; the sequential fallback
///          needs the complete cache and may still emit every individually representable component.
template <::std::integral char_type, typename... Args>
inline constexpr ::std::size_t concat_measure_mixed_components_once(
	::fast_io::details::decay::concat_mixed_component_cache<char_type> *cache, Args &...args)
{
	::std::size_t total{};
	::std::size_t index{};
	auto measure_one = [&cache, &total, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		auto &component{cache[index++]};
		::std::size_t extent{};
		if constexpr (::fast_io::reserve_printable<char_type, arg_type>)
		{
			component.scatter.len = print_reserve_size(::fast_io::io_reserve_type<char_type, arg_type>);
			extent = component.scatter.len;
		}
		else if constexpr (::fast_io::dynamic_reserve_printable<char_type, arg_type>)
		{
			component.scatter.len = print_reserve_size(
				::fast_io::io_reserve_type<char_type, arg_type>, arg);
			extent = component.scatter.len;
		}
		else
		{
			// Spell the retained-scatter proof here because the public convenience trait is declared below this planning
			// machinery. This is the same exact named-lvalue expression and independent lifetime/replay contract.
			static_assert(::fast_io::scatter_printable_for<char_type, arg_type &> &&
						  ::fast_io::borrowed_scatter_source<char_type, arg_type>);
			component.scatter = print_scatter_define(
				::fast_io::io_reserve_type<char_type, arg_type>, arg);
			extent = component.scatter.len;
		}
		if (::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(0u, extent) == SIZE_MAX)
			[[unlikely]]
		{
			// One component alone cannot fit a C++ contiguous range. No aggregate or sequential strategy can make
			// its reserve contract or exact scatter length valid, so stop before forming any derived pointer.
			::fast_io::fast_terminate();
		}
		total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(total, extent);
	};
	(measure_one(args), ...);
	return total;
}

/// @brief Emits a cached concat plan either into one exact aggregate allocation or sequentially through one scratch.
/// @details Successful aggregation and the unavailable-plan fallback consume the same cached size/scatter metadata;
///          neither path replays a size or scatter CPO. The grouped path validates every reserve producer's returned
///          cursor against its own cached slice before advancing. If the aggregate is unavailable, one scratch buffer
///          sized to the maximum reserve bound is reused in argument order and only each producer's actual prefix is
///          appended to the growable destination. Scatter descriptors are consumed directly and a newline is appended
///          only after all cached components. This preserves the size-then-define contract and does not depend on a
///          second size/scatter query; retained scatters still carry their independent repeatability proof.
template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_cached_mixed_with_storage(
	::fast_io::details::decay::concat_mixed_component_cache<char_type> *cache, T &str, Args &...args)
{
	::std::size_t total{
		::fast_io::details::decay::concat_measure_mixed_components_once<char_type>(cache, args...)};
	total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		total, static_cast<::std::size_t>(line));
	if (total != SIZE_MAX) [[likely]]
	{
		if constexpr (::fast_io::sso_buffer_strlike<char_type, T>)
		{
			constexpr ::std::size_t local_capacity{
				strlike_sso_size(::fast_io::io_strlike_type<char_type, T>)};
			if (local_capacity < total)
			{
				strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
			}
		}
		else
		{
			strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
		}

		char_type *current{strlike_begin(::fast_io::io_strlike_type<char_type, T>, str)};
		::std::size_t index{};
		auto emit_one = [cache, &current, &index](auto &arg) constexpr {
			using arg_type = ::std::remove_cvref_t<decltype(arg)>;
			auto const &component{cache[index++]};
			if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
						  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
			{
				char_type *const component_last{
					component.scatter.len == 0u ? current : current + component.scatter.len};
				char_type *const result{print_reserve_define(
					::fast_io::io_reserve_type<char_type, arg_type>, current, arg)};
				if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
						current, component_last, result)) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				current = result;
			}
			else
			{
				if (component.scatter.len != 0u)
				{
					current = ::fast_io::details::decay::small_scatter_copy_n(
						component.scatter.base, component.scatter.len, current);
				}
			}
		};
		(emit_one(args), ...);
		if constexpr (line)
		{
			*current++ = ::fast_io::char_literal_v<u8'\n', char_type>;
		}
		strlike_set_curr(::fast_io::io_strlike_type<char_type, T>, str, current);
		return;
	}

	::std::size_t maximum_reserve_bound{};
	::std::size_t index{};
	auto observe_bound = [cache, &maximum_reserve_bound, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		auto const &component{cache[index++]};
		if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
					  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
		{
			if (maximum_reserve_bound < component.scatter.len)
			{
				maximum_reserve_bound = component.scatter.len;
			}
		}
	};
	(observe_bound(args), ...);

	// This branch is selected only for an exact writable-buffer result: the aggregate bound was unavailable, not the
	// result's cursor protocol. Use fast_io's maintained cursor adapter directly. An associated `io_strlike_ref` is not
	// evidence that its result supports primitive writes (it may be an unrelated or malformed customization), and
	// requiring such a customization here made a valid buffer-only result fail only on the rare overflow fallback.
	::fast_io::io_strlike_reference_wrapper<char_type, T> destination{__builtin_addressof(str)};
	auto emit_sequentially = [&](char_type *scratch) constexpr {
		::std::size_t component_index{};
		auto emit_one = [cache, &destination, &component_index, scratch](auto &arg) constexpr {
			using arg_type = ::std::remove_cvref_t<decltype(arg)>;
			auto const &component{cache[component_index++]};
			if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
						  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
			{
				char_type *const scratch_last{
					component.scatter.len == 0u ? scratch : scratch + component.scatter.len};
				char_type *const result{print_reserve_define(
					::fast_io::io_reserve_type<char_type, arg_type>, scratch, arg)};
				if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
						scratch, scratch_last, result)) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				::fast_io::operations::decay::write_all_decay_dispatch(destination, scratch, result);
			}
			else if (component.scatter.len != 0u)
			{
				::fast_io::operations::decay::write_all_decay_dispatch(
					destination, component.scatter.base, component.scatter.base + component.scatter.len);
			}
		};
		(emit_one(args), ...);
	};

	if (maximum_reserve_bound == 0u)
	{
		char_type scratch{};
		emit_sequentially(__builtin_addressof(scratch));
	}
	else
	{
		// Aggregate unavailability implies that this branch is already exceptional. One reusable dynamic scratch keeps
		// the frame bounded and avoids the sum-of-upper-bounds allocation that this fallback exists to reject. Every
		// component was checked against the contiguous character limit while the cache was built, but this actual object
		// allocation needs the tighter byte-domain proof `count * sizeof(char_type) <= PTRDIFF_MAX`. GCC 13/15 do not retain
		// either range fact through the two generic lambdas above and otherwise diagnose the allocation as potentially near
		// SIZE_MAX under -Walloc-size-larger-than. Repeat the fail-closed allocation limit here: it executes only after
		// aggregate overflow selected this cold fallback and gives both the allocator and later pointer arithmetic one local
		// proof without adding work to the successful aggregate path.
		constexpr ::std::size_t maximum_scratch_chars{
			static_cast<::std::size_t>(PTRDIFF_MAX) / sizeof(char_type)};
		if (maximum_scratch_chars < maximum_reserve_bound) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		FAST_IO_ASSUME(maximum_reserve_bound <= maximum_scratch_chars);
		::fast_io::details::local_operator_new_array_ptr<char_type> scratch(maximum_reserve_bound);
		emit_sequentially(scratch.ptr);
	}
	if constexpr (line)
	{
		::fast_io::operations::decay::char_put_decay_dispatch(
			destination, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

/// @brief Applies the shared stack-byte policy to cached mixed-concat metadata.
/// @details A component cache is an optimization frame, not part of the printable protocol. An argument pack may be
///          arbitrarily long, so placing one two-word scatter record per leaf on the stack would make concept
///          composition itself an unbounded frame-size hazard. Small plans retain the allocation-free array; large
///          plans use one exact dynamic metadata allocation and then execute the identical measurement and emission
///          implementation.
template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_cached_mixed(T &str, Args &...args)
{
	using cache_type = ::fast_io::details::decay::concat_mixed_component_cache<char_type>;
	static_assert(sizeof...(Args) != 0u);
	if constexpr (
		::fast_io::details::decay::concat_metadata_stack_safe<sizeof...(Args), cache_type>())
	{
		cache_type cache[sizeof...(Args)]{};
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed_with_storage<line, char_type>(
			cache, str, args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<cache_type> cache(sizeof...(Args));
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed_with_storage<line, char_type>(
			cache.ptr, str, args...);
	}
}

/// @brief Classifies a concat leaf whose selected reserve protocol is exclusively object-dependent.
/// @details The general planner gives type-level reserve precedence over dynamic reserve, and the dispatcher gives an
///          all-retained-scatter run precedence over either representation. Requiring the absence of the static reserve
///          protocol makes the compact plan below observationally identical even for a type that happens to expose
///          several CPO families: every admitted leaf is measured through the same dynamic CPO that the mixed planner
///          would select. A retained scatter on such a leaf remains irrelevant unless the complete run selected the
///          earlier all-scatter strategy.
template <::std::integral char_type, typename T>
inline constexpr bool concat_exclusive_dynamic_reserve_leaf_v =
	::fast_io::dynamic_reserve_printable<char_type, T> &&
	!::fast_io::reserve_printable<char_type, T>;

/// @brief Proves that a fresh destination exposes an audited run-time put area for bounded direct construction.
/// @details The marker certifies that reserving capacity creates writable run-time storage without publishing logical
///          characters, and that one final cursor commit establishes exactly the completed prefix. The three operation
///          expressions are tested independently so the marker cannot manufacture a missing adapter capability.
template <typename char_type, typename T>
concept basic_general_concat_runtime_dynamic_direct_destination =
	::std::integral<char_type> && requires(T &str, ::std::size_t size, char_type *cursor) {
		{
			strlike_concat_fresh_runtime_dynamic_direct_safe(
				::fast_io::io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
		strlike_runtime_reserve(::fast_io::io_strlike_type<char_type, T>, str, size);
		{
			strlike_runtime_begin(::fast_io::io_strlike_type<char_type, T>, str)
		} -> ::std::same_as<char_type *>;
		strlike_runtime_set_curr(::fast_io::io_strlike_type<char_type, T>, str, cursor);
	};

/// @brief Measures a pure dynamic-reserve pack exactly once into compact bound storage.
/// @details The fold is sequenced left-to-right. Every component is first proved representable as one contiguous
///          character extent, then participates in a saturating aggregate. Measurement continues after aggregate
///          unavailability because the sequential fallback requires all one-shot bounds and must not replay a CPO.
template <::std::integral char_type, typename First, typename... Rest>
	requires(concat_exclusive_dynamic_reserve_leaf_v<char_type, First> &&
			 (concat_exclusive_dynamic_reserve_leaf_v<char_type, Rest> && ...))
inline constexpr ::std::size_t concat_measure_dynamic_components_once(
	::std::size_t *bounds, First &first, Rest &...rest)
{
	::std::size_t total{};
	::std::size_t index{};
	auto measure_one = [bounds, &total, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		::std::size_t const bound{print_reserve_size(
			::fast_io::io_reserve_type<char_type, arg_type>, arg)};
		bounds[index++] = bound;
		if (::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
				0u, bound) == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
			total, bound);
	};
	if constexpr (8u <= 1u + sizeof...(Rest) &&
				  (::std::same_as<::std::remove_reference_t<First>,
								  ::std::remove_reference_t<Rest>> &&
				   ...))
	{
		// One loop body prevents GCC from cloning the same object-dependent query and overflow graph for every leaf.
		// The pointer table preserves the original named-object order and never changes the decay transport ABI.
		using expression_type = ::std::remove_reference_t<First>;
		expression_type *arguments[]{__builtin_addressof(first), __builtin_addressof(rest)...};
		for (auto *argument : arguments)
		{
			measure_one(*argument);
		}
	}
	else
	{
		measure_one(first);
		(measure_one(rest), ...);
	}
	return total;
}

/// @brief Emits a compact dynamic plan into one already-reserved contiguous range.
/// @details A homogeneous expression pack shares one CPO/copy loop through an ordered pointer table; a heterogeneous
///          pack retains compile-time dispatch through the fold. Both forms validate every returned endpoint against
///          its own cached capacity before it becomes the next leaf's cursor. Thus the loop is a code-generation
///          quotient over identical types, not a weakening of the per-component reserve contract.
template <::std::integral char_type, typename First, typename... Rest>
	requires(concat_exclusive_dynamic_reserve_leaf_v<char_type, First> &&
			 (concat_exclusive_dynamic_reserve_leaf_v<char_type, Rest> && ...))
inline constexpr char_type *concat_emit_dynamic_components_to_contiguous(
	::std::size_t const *bounds, char_type *current, First &first, Rest &...rest)
{
	::std::size_t index{};
	auto emit_one = [bounds, &current, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		::std::size_t const bound{bounds[index++]};
		char_type *const component_last{bound == 0u ? current : current + bound};
		char_type *const emitted_end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, arg_type>, current, arg)};
		if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
				current, component_last, emitted_end)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		current = emitted_end;
	};
	if constexpr (8u <= 1u + sizeof...(Rest) &&
				  (::std::same_as<::std::remove_reference_t<First>,
								  ::std::remove_reference_t<Rest>> &&
				   ...))
	{
		using expression_type = ::std::remove_reference_t<First>;
		expression_type *arguments[]{__builtin_addressof(first), __builtin_addressof(rest)...};
		for (auto *argument : arguments)
		{
			emit_one(*argument);
		}
	}
	else
	{
		emit_one(first);
		(emit_one(rest), ...);
	}
	return current;
}

/// @brief Emits one cached dynamic-reserve component through the aggregate-overflow fallback.
/// @details Keeping this leaf operation in a cold, non-inlined template lets identical normalized source types share
///          one writer body. GCC otherwise outlines the generic lambda from the pack-level fallback as one large hot
///          `.text` function containing every copy loop, even though aggregate overflow is exceptional. The helper
///          retains the exact cached-bound cursor proof and appends only the producer's returned prefix.
template <::std::integral char_type, typename T, typename Arg>
#if defined(__GNUC__)
[[gnu::cold, gnu::noinline]]
#endif
inline constexpr void basic_general_concat_cached_dynamic_overflow_emit_one(
	::fast_io::io_strlike_reference_wrapper<char_type, T> &destination,
	::std::size_t bound, char_type *scratch, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	char_type *const scratch_last{bound == 0u ? scratch : scratch + bound};
	char_type *const result{print_reserve_define(
		::fast_io::io_reserve_type<char_type, arg_type>, scratch, arg)};
	if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			scratch, scratch_last, result)) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	::fast_io::operations::decay::write_all_decay_dispatch(destination, scratch, result);
}

/// @brief Completes a pure dynamic-reserve concat after its aggregate bound proved unavailable.
/// @details Each entry in `bounds` is the result of the one and only size-CPO invocation for its corresponding named
///          decay object. The fallback therefore cannot replay a stateful measurement. It reuses one scratch range of
///          the maximum component capacity, validates every writer cursor against that component's cached bound, and
///          appends only the returned prefix. This is the same formal protocol as the mixed fallback, separated so its
///          allocation and adapter machinery does not consume GCC's inlining budget or instruction cache on the
///          overwhelmingly common representable aggregate path.
template <bool line, ::std::integral char_type, typename T, typename... Args>
#if defined(__GNUC__)
[[gnu::cold, gnu::noinline]]
#endif
inline constexpr void basic_general_concat_decay_ref_impl_cached_dynamic_overflow(
	::std::size_t const *bounds, T &str, Args &...args)
{
	::std::size_t maximum_bound{};
	for (::std::size_t index{}; index != sizeof...(Args); ++index)
	{
		if (maximum_bound < bounds[index])
		{
			maximum_bound = bounds[index];
		}
	}

	::fast_io::io_strlike_reference_wrapper<char_type, T> destination{__builtin_addressof(str)};
	auto emit_all = [&](char_type *scratch) constexpr {
		::std::size_t index{};
		(::fast_io::details::decay::basic_general_concat_cached_dynamic_overflow_emit_one(
			 destination, bounds[index++], scratch, args),
		 ...);
	};

	if (maximum_bound == 0u)
	{
		char_type scratch{};
		emit_all(__builtin_addressof(scratch));
	}
	else
	{
		constexpr ::std::size_t maximum_scratch_chars{
			static_cast<::std::size_t>(PTRDIFF_MAX) / sizeof(char_type)};
		if (maximum_scratch_chars < maximum_bound) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		FAST_IO_ASSUME(maximum_bound <= maximum_scratch_chars);
		::fast_io::details::local_operator_new_array_ptr<char_type> scratch(maximum_bound);
		emit_all(scratch.ptr);
	}
	if constexpr (line)
	{
		::fast_io::operations::decay::char_put_decay_dispatch(
			destination, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

/// @brief Coalesces a pure dynamic-reserve run using one machine-word bound per decay object.
/// @details The mixed planner needs a two-word scatter descriptor because each type may select reserve or retained
///          scatter. That representation is redundant when every selected leaf is dynamic reserve. This specialization
///          retains the semantically necessary per-leaf bounds (one-shot size observation, overflow fallback, and
///          returned-cursor proof) but removes unused bases and run-time component indexing from the hot measurement
///          fold. `Args&` deliberately borrows the phase-1 decay objects: introducing another by-value layer here would
///          both replay transport copies and replace ABI-selected value normalization with reference transport at the
///          public boundary.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires((sizeof...(Args) <= 64u) &&
			 (concat_exclusive_dynamic_reserve_leaf_v<char_type, Args> && ...))
inline constexpr void basic_general_concat_decay_ref_impl_cached_dynamic(T &str, Args &...args)
{
	static_assert(sizeof...(Args) != 0u);
	::std::size_t bounds[sizeof...(Args)]{};
	::std::size_t total{
		::fast_io::details::decay::concat_measure_dynamic_components_once<char_type>(
			bounds, args...)};
	total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		total, static_cast<::std::size_t>(line));
	if (total == SIZE_MAX) [[unlikely]]
	{
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_dynamic_overflow<line, char_type>(
			bounds, str, args...);
		return;
	}

	if constexpr (::fast_io::sso_buffer_strlike<char_type, T>)
	{
		constexpr ::std::size_t local_capacity{
			strlike_sso_size(::fast_io::io_strlike_type<char_type, T>)};
		if (local_capacity < total)
		{
			strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
		}
	}
	else
	{
		strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
	}

	char_type *current{
		::fast_io::details::decay::concat_emit_dynamic_components_to_contiguous<char_type>(
			bounds, strlike_begin(::fast_io::io_strlike_type<char_type, T>, str), args...)};
	if constexpr (line)
	{
		*current++ = ::fast_io::char_literal_v<u8'\n', char_type>;
	}
	strlike_set_curr(::fast_io::io_strlike_type<char_type, T>, str, current);
}

/// @brief Constructs a fresh audited run-time destination directly from cached dynamic bounds.
/// @details Sizing precedes destination allocation and each size CPO is invoked once. A representable aggregate is
///          reserved in the destination's certified unpublished put area; writers advance through disjoint bounded
///          slices, and the final cursor is committed only after the entire run succeeds. If the conservative sum is
///          unavailable, the same cached bounds feed the sequential concat-buffer fallback, so no stateful size query
///          is replayed. This function borrows the phase-1 decay objects and therefore does not alter their value-based
///          public ABI normalization.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_runtime_dynamic_direct_destination<char_type, T> &&
		(sizeof...(Args) <= 64u) &&
		(concat_exclusive_dynamic_reserve_leaf_v<char_type, Args> && ...))
inline constexpr T basic_general_concat_fresh_runtime_dynamic_direct(Args &...args)
{
	static_assert(sizeof...(Args) != 0u);
	::std::size_t bounds[sizeof...(Args)]{};
	::std::size_t total{
		::fast_io::details::decay::concat_measure_dynamic_components_once<char_type>(
			bounds, args...)};
	total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		total, static_cast<::std::size_t>(line));
	if (total == SIZE_MAX) [[unlikely]]
	{
		::fast_io::details::basic_concat_buffer<char_type> buffer;
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_dynamic_overflow<line, char_type>(
			bounds, buffer, args...);
		return strlike_construct_define(
			::fast_io::io_strlike_type<char_type, T>, buffer.buffer_begin, buffer.buffer_curr);
	}

	T result;
	strlike_runtime_reserve(::fast_io::io_strlike_type<char_type, T>, result, total);
	char_type *current{
		::fast_io::details::decay::concat_emit_dynamic_components_to_contiguous<char_type>(
			bounds,
			strlike_runtime_begin(::fast_io::io_strlike_type<char_type, T>, result),
			args...)};
	if constexpr (line)
	{
		*current++ = ::fast_io::char_literal_v<u8'\n', char_type>;
	}
	strlike_runtime_set_curr(
		::fast_io::io_strlike_type<char_type, T>, result, current);
	return result;
}

/// @brief Emits a small mixed reserve/scatter run without the general-purpose planning cache.
/// @details The generic mixed planner handles arbitrary pack lengths and stateful dynamic producers. Literal/string
/// records are more common and can use one bounded descriptor array instead: each retained scatter is queried once,
/// static reserve leaves use their compile-time extent, and the exact aggregate is then emitted in source order.
#if defined(__clang__)
template <::std::integral ch_type, typename T>
inline constexpr bool concat_small_mixed_explicit_scatter_v =
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::basic_io_scatter_t<ch_type>> ||
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::basic_prfch_cacheable_io_scatter_t<ch_type>> ||
	::fast_io::details::decay::print_static_scatter_traits<ch_type, ::std::remove_cvref_t<T>>::available;

template <::std::integral ch_type, typename T>
inline constexpr bool concat_small_mixed_reserve_leaf_v =
	!concat_small_mixed_explicit_scatter_v<ch_type, T> &&
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> &&
	requires(::std::remove_cvref_t<T> &value, ch_type *ptr) {
		{
			print_reserve_define(
				::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<T>>, ptr, value)
		} -> ::std::same_as<ch_type *>;
	};

template <::std::integral ch_type, typename T>
inline constexpr bool concat_small_mixed_leaf_v =
	concat_small_mixed_reserve_leaf_v<ch_type, T> ||
	concat_small_mixed_explicit_scatter_v<ch_type, T>;

template <::std::integral ch_type, typename T>
inline constexpr bool concat_small_mixed_scatter_leaf_v =
	concat_small_mixed_explicit_scatter_v<ch_type, T> &&
	::fast_io::details::decay::retained_scatter_printable_v<ch_type, T>;

template <::std::integral ch_type, typename... Args>
inline constexpr bool concat_small_mixed_run_v =
	(sizeof...(Args) > 1u && sizeof...(Args) <= 8u) &&
	(concat_small_mixed_leaf_v<ch_type, Args> && ...) &&
	(concat_small_mixed_scatter_leaf_v<ch_type, Args> || ...);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
basic_general_concat_decay_ref_impl_small_mixed(T &str, Args &...args)
{
	basic_io_scatter_t<ch_type> scatters[sizeof...(Args)]{};
	::std::size_t scatter_index{};
	::std::size_t total_size{static_cast<::std::size_t>(line)};
	auto measure_one = [&scatters, &scatter_index, &total_size](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		using static_scatter_traits =
			::fast_io::details::decay::print_static_scatter_traits<ch_type, arg_type>;
		::std::size_t extent{};
		if constexpr (concat_small_mixed_reserve_leaf_v<ch_type, arg_type>)
		{
			extent = print_reserve_size(::fast_io::io_reserve_type<ch_type, arg_type>);
		}
		else if constexpr (static_scatter_traits::available)
		{
			// Admission and execution must use the same protocol witness.  Formally, a static-trait leaf enters
			// this plan because `static_scatter_traits::available`; its descriptor is therefore exactly
			// `static_scatter_traits::define(arg)`.  Such a leaf intentionally need not expose the dynamic
			// `print_scatter_define` CPO, so substituting that CPO here would make a proven leaf ill-formed.
			scatters[scatter_index] = static_scatter_traits::define(arg);
			extent = scatters[scatter_index].len;
		}
		else
		{
			scatters[scatter_index] = print_scatter_define(
				::fast_io::io_reserve_type<ch_type, arg_type>, arg);
			extent = scatters[scatter_index].len;
		}
		++scatter_index;
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
			total_size, extent);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	};
	(measure_one(args), ...);
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto current{strlike_begin(io_strlike_type<ch_type, T>, str)};
	scatter_index = 0u;
	auto emit_one = [&current, &scatters, &scatter_index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		if constexpr (concat_small_mixed_reserve_leaf_v<ch_type, arg_type>)
		{
			current = print_reserve_define(
				::fast_io::io_reserve_type<ch_type, arg_type>, current, arg);
		}
		else
		{
			auto const &scatter{scatters[scatter_index]};
			if (scatter.len != 0u)
			{
				current = ::fast_io::details::decay::small_scatter_copy_n(scatter.base, scatter.len, current);
			}
		}
		++scatter_index;
	};
	(emit_one(args), ...);
	if constexpr (line)
	{
		*current++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, current);
}
#endif

template <::std::integral ch_type, typename T>
inline constexpr basic_io_scatter_t<ch_type> print_scatter_define_extract_one(T &t)
{
	// This helper returns the descriptor to its caller, so it must borrow the phase-1 owner rather than create another
	// formatter copy. A valid borrowed producer may point into itself; returning a descriptor into a by-value local
	// would end that storage lifetime before the string constructor consumes the range.
	return print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<T>>, t);
}

/// @brief Tests the retained byte threshold without multiplying a descriptor length by its character width.
/// @details `ceil(minimum_bytes / sizeof(ch_type))` is the exact minimum character count whose object representation
///          spans the required byte extent. Division and remainder keep the proof valid even when an adversarial
///          descriptor length would overflow `size_t` if multiplied by `sizeof(ch_type)`.
template <::std::integral ch_type>
inline constexpr bool concat_scatter_payload_meets_read_prfch_threshold(::std::size_t length) noexcept
{
	constexpr ::std::size_t character_width{sizeof(ch_type)};
	constexpr ::std::size_t minimum_bytes{
		::fast_io::concat_scatter_chain_read_prfch_minimum_payload_bytes};
	constexpr ::std::size_t minimum_characters{
		minimum_bytes / character_width + static_cast<::std::size_t>(minimum_bytes % character_width != 0u)};
	return minimum_characters <= length;
}

/// @brief Tests the complete payload-byte domain selected for one concrete platform.
/// @details The multiplication is performed only after a division guard, so an adversarial character count cannot
///          overflow. The historical minimum-only helper above remains available for source compatibility; production
///          concat uses this complete predicate, including Apple's retained discrete payload sizes.
template <::fast_io::prfch_platform platform_type, ::std::integral ch_type>
inline constexpr bool concat_scatter_payload_meets_read_prfch_policy(
	::std::size_t length) noexcept
{
	constexpr ::std::size_t character_width{sizeof(ch_type)};
	if (SIZE_MAX / character_width < length)
	{
		return false;
	}
	return ::fast_io::concat_scatter_chain_read_prfch_payload_bytes_eligible<platform_type>(
		length * character_width);
}

/// @brief Sums an exact scatter prefix and, when admitted, classifies read-prefetch eligibility in the same traversal.
/// @details The strategy-enabled run checks the platform policy's complete nonempty-descriptor and payload domains.
///          The x86 hybrid rule is a minimum-only 32 by four-KiB boundary. Apple admits only counts 512/768/1024 with
///          one uniform payload size of 1024 or 2048 bytes. No payload base is inspected here. A count at the platform
///          maximum saturates and separately records any later nonempty descriptor, while the byte predicate guards
///          multiplication, so neither statistic can overflow.
///
///          Constant evaluation and compile-time-disabled policies execute the historical summation loop below. They
///          construct no eligibility counters and perform no lookahead; `runtime_read_prfch` remains false. At run time
///          an admitted policy folds the two statistics into the summation that concat already required, avoiding a
///          third descriptor pass before copying.
template <bool read_prfch,
		  typename platform_type = ::fast_io::details::native_prfch_platform,
		  ::std::integral ch_type>
	requires(::fast_io::prfch_platform<platform_type>)
inline constexpr ::std::size_t calculate_scatter_total_size(
	basic_io_scatter_t<ch_type> const *first, basic_io_scatter_t<ch_type> const *last,
	bool &runtime_read_prfch)
{
	runtime_read_prfch = false;
	if constexpr (read_prfch)
	{
		if (!::std::is_constant_evaluated())
		{
			auto constexpr policy{
				::fast_io::concat_scatter_chain_read_prfch_policy_for<platform_type>};
			::std::size_t total_size{};
			::std::size_t nonempty_count{};
			[[maybe_unused]] ::std::size_t first_nonempty_length{};
			bool descriptor_count_did_not_exceed_maximum{true};
			bool every_nonempty_payload_is_in_range{true};
			bool every_nonempty_payload_has_one_size{true};
			for (; first != last; ++first)
			{
				::std::size_t const length{first->len};
				total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					total_size, length);
				if (total_size == SIZE_MAX) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				if (length != 0u)
				{
					if constexpr (
						::fast_io::concat_scatter_chain_read_prfch_requires_uniform_payload<platform_type>)
					{
						if (nonempty_count == 0u)
						{
							first_nonempty_length = length;
						}
						else
						{
							every_nonempty_payload_has_one_size =
								every_nonempty_payload_has_one_size && length == first_nonempty_length;
						}
					}
					if (nonempty_count == policy.maximum_descriptor_count)
					{
						descriptor_count_did_not_exceed_maximum = false;
					}
					else
					{
						++nonempty_count;
					}
					every_nonempty_payload_is_in_range =
						every_nonempty_payload_is_in_range &&
						::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_policy<
							platform_type, ch_type>(length);
				}
			}
			runtime_read_prfch =
				descriptor_count_did_not_exceed_maximum && every_nonempty_payload_is_in_range &&
				(!::fast_io::concat_scatter_chain_read_prfch_requires_uniform_payload<platform_type> ||
				 every_nonempty_payload_has_one_size) &&
				::fast_io::concat_scatter_chain_read_prfch_descriptor_count_eligible<platform_type>(
					nonempty_count);
			return total_size;
		}
	}
	::std::size_t total_size{};
	for (; first != last; ++first)
	{
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
			total_size, first->len);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			// Scatter lengths are exact output, not optional reserve bounds. An unrepresentable exact total cannot be
			// split into one result object, so reject it before allocation or any base-plus-length expression is formed.
			::fast_io::fast_terminate();
		}
	}
	return total_size;
}

/// @brief Copies an exact retained scatter prefix, optionally applying concat's measured next-source read hint.
/// @details The optimized loop is instantiated only after the caller proves platform, descriptor-capacity, and every
///          normalized producer's cacheable-read provenance. The explicit run-time gate comes from the existing size
///          traversal and proves the actual chain lies inside the selected platform's complete descriptor/payload
///          domain. A false gate therefore enters the original loop without executing a next-descriptor search.
///          When true, empty descriptors are skipped by length before their `base` member is ever evaluated, and the
///          only hinted expression is exactly the next nonempty descriptor's live-range base.
///
///          Constant evaluation deliberately executes the original single traversal: it performs neither a prefetch
///          nor the next-nonempty search. At run time, three Linux P-core seeds retained about 1--2.4 percent for cold
///          discontinuous 4--16 KiB chains and about +/-0.2 percent for the hot controls. The independent M4 P/E-core
///          intersection uses only counts 512/768/1024 with uniform 1024- or 2048-byte payloads. These different
///          domains are why site policy remains independent from broad platform capability and why no write hint is
///          mirrored here.
template <bool read_prfch,
		  typename platform_type = ::fast_io::details::native_prfch_platform,
		  ::std::integral ch_type>
	requires(::fast_io::prfch_platform<platform_type>)
inline constexpr ch_type *copy_scatter_chain_to_buffer(ch_type *iter, basic_io_scatter_t<ch_type> const *first,
													   basic_io_scatter_t<ch_type> const *last,
													   bool runtime_read_prfch)
{
	if constexpr (read_prfch)
	{
		if (!::std::is_constant_evaluated() && runtime_read_prfch)
		{
			auto constexpr policy{
				::fast_io::concat_scatter_chain_read_prfch_policy_for<platform_type>};
			auto current{first};
			while (current != last && current->len == 0u)
			{
				// A zero-length descriptor carries no live payload premise; in particular, its base may be null.
				++current;
			}
			while (current != last)
			{
				auto next{current + 1u};
				while (next != last && next->len == 0u)
				{
					++next;
				}
				if (next != last)
				{
					::fast_io::prfch<::fast_io::prfch_mode::read, policy.level,
									 policy.retention>(next->base);
				}
				iter = ::fast_io::details::decay::small_scatter_copy_n(current->base, current->len, iter);
				current = next;
			}
			return iter;
		}
	}
	for (; first != last; ++first)
	{
		if (first->len != 0u)
		{
			// Preserve the same empty-descriptor rule in the baseline and constexpr paths.
			iter = ::fast_io::details::decay::small_scatter_copy_n(first->base, first->len, iter);
		}
	}
	return iter;
}

template <::std::integral ch_type, typename T>
inline constexpr bool direct_scatter_view_printable =
	::std::same_as<::std::remove_cvref_t<T>, basic_io_scatter_t<ch_type>>;

/// @brief Proves that concat may retain a produced scatter while it queries later arguments.
/// @details Multi-scatter concat first collects every descriptor and only then allocates and copies the complete
///          result. That ordering is valid for a raw caller-owned scatter view and for explicitly borrowed producers,
///          but not for an arbitrary scatter CPO backed by shared scratch storage. Unproved producers leave this
///          optimized branch and are emitted sequentially through the ordinary string-output dispatcher, which
///          consumes each descriptor before invoking the next producer. Concat owns normalized arguments once in its
///          phase-1 entry and every lower helper borrows those named objects; `T&` is therefore the exact expression
///          category whose overload must exist. Testing the public forwarding query here would both admit an rvalue-only
///          CPO that the body cannot call and miss a valid lvalue-only producer.
template <::std::integral ch_type, typename T>
inline constexpr bool concat_retained_scatter_printable_v =
	::fast_io::scatter_printable_for<ch_type, T &> &&
	::fast_io::borrowed_scatter_source<ch_type, T>;

#if defined(__GNUC__) && !defined(__clang__)
/// GCC's cost model can leave a cached mixed concat leaf out of its caller
/// when a translation unit also contains several large formatting records.
/// Restrict the flatten escape hatch to a small normalized reserve/scatter
/// pack with at least one retained scatter; generic dynamic producers and
/// unbounded packs retain the ordinary call boundary.
template <::std::integral ch_type, typename T>
inline constexpr bool concat_gcc_hot_mixed_leaf_v =
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	concat_retained_scatter_printable_v<ch_type, T>;

template <::std::integral ch_type, typename... Args>
inline constexpr bool concat_gcc_hot_mixed_run_v =
	(sizeof...(Args) >= 2u && sizeof...(Args) <= 8u) &&
	(concat_gcc_hot_mixed_leaf_v<ch_type, Args> && ...) &&
	(concat_retained_scatter_printable_v<ch_type, Args> || ...) &&
	!(::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<Args>> && ...);

#endif

#if defined(__GNUC__) && !defined(__clang__)
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires(concat_gcc_hot_mixed_run_v<char_type, Args...>)
FAST_IO_GNU_ALWAYS_INLINE
	[[gnu::flatten]]
	inline constexpr void basic_general_concat_decay_ref_impl_cached_mixed_gcc_hot(T &str, Args &...args)
{
	basic_general_concat_decay_ref_impl_cached_mixed<line, char_type>(str, args...);
}
#endif

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_direct(T &str, Args &...args)
{
	::std::size_t total_size{};
	auto add_exact_length = [&total_size](::std::size_t length) constexpr {
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(total_size, length);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	};
	(add_exact_length(args.len), ...);
	if constexpr (line)
	{
		add_exact_length(1u);
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto ptr{strlike_begin(io_strlike_type<ch_type, T>, str)};
	((ptr = ::fast_io::details::decay::small_scatter_copy_n(args.base, args.len, ptr)), ...);
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage(
	::fast_io::basic_io_scatter_t<ch_type> *scatters, T &str, Args &...args)
{
	constexpr bool read_prfch{
		::fast_io::concat_scatter_chain_read_prfch_strategy<
			::fast_io::details::native_prfch_platform, sizeof...(Args), Args...>};
	::std::size_t scatter_index{};
	((scatters[scatter_index++] =
		  print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>, args)),
	 ...);
	bool runtime_read_prfch{};
	::std::size_t total_size{calculate_scatter_total_size<read_prfch>(
		scatters, scatters + sizeof...(Args), runtime_read_prfch)};
	if constexpr (line)
	{
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(total_size, 1u);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto ptr{copy_scatter_chain_to_buffer<read_prfch>(
		strlike_begin(io_strlike_type<ch_type, T>, str), scatters, scatters + sizeof...(Args),
		runtime_read_prfch)};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief Materializes generic retained scatters without making pack length an unbounded stack cost.
/// @details Generic producers must be queried before destination allocation because their descriptors may point into
///          the normalized owner. The descriptor table is therefore necessary, but its storage class is only a cost
///          decision: compact tables stay local while large flattened packs use one exact metadata allocation.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_generic(T &str, Args &...args)
{
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	if constexpr (
		::fast_io::details::decay::concat_metadata_stack_safe<sizeof...(Args), scatter_type>())
	{
		scatter_type scatters[sizeof...(Args)];
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage<
			line, ch_type>(scatters, str, args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(sizeof...(Args));
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage<
			line, ch_type>(scatters.ptr, str, args...);
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter(T &str, Args &...args)
{
	if constexpr ((direct_scatter_view_printable<ch_type, Args> && ...))
	{
		basic_general_concat_decay_ref_impl_all_scatter_direct<line, ch_type>(str, args...);
	}
	else
	{
		basic_general_concat_decay_ref_impl_all_scatter_generic<line, ch_type>(str, args...);
	}
}

template <bool line>
inline constexpr ::std::size_t concat_precise_size_with_line(::std::size_t precise_size);

/// @brief Proves that concat may retain every descriptor produced by one static reserve-scatters leaf.
/// @details The reserve-scatters shape concept proves only compile-time capacities.  The independent borrowed-source
///          marker proves the missing lifetime property: invoking a later producer cannot invalidate descriptors
///          already collected from this object.  Without that marker concat must consume each object immediately.
template <::std::integral ch_type, typename T>
inline constexpr bool concat_retained_reserve_scatters_printable_v =
	::fast_io::reserve_scatters_printable<ch_type, ::std::remove_cvref_t<T>> &&
	::fast_io::details::decay::retained_reserve_scatters_printable_v<
		ch_type, ::std::remove_cvref_t<T>>;

/// @brief Computes the aggregate descriptor and reserve-storage capacities of a retained concat plan.
/// @details Both dimensions use the common contiguous-extent proof.  `SIZE_MAX` is an unavailable-plan sentinel, not
///          an allocation request: it records either arithmetic overflow or a sum that cannot participate in C++
///          array pointer arithmetic.  Individual reserve-scatters concepts already prove constant evaluation.
template <::std::integral ch_type, typename... Args>
inline consteval ::fast_io::reserve_scatters_size_result
concat_retained_reserve_scatters_capacity() noexcept
{
	::fast_io::reserve_scatters_size_result result{};
	((result.scatters_size = ::fast_io::details::decay::print_strategy_extent_add_or_unavailable(
		  result.scatters_size,
		  print_reserve_scatters_size(
			  ::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>)
			  .scatters_size),
	  result.reserve_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
		  result.reserve_size,
		  print_reserve_scatters_size(
			  ::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>)
			  .reserve_size)),
	 ...);
	return result;
}

/// @brief Selects the retained static reserve-scatters concat plan only when both arrays are representable.
/// @details Descriptor count and descriptor bytes are different bounds.  A count below `PTRDIFF_MAX` can still
///          overflow `count * sizeof(scatter)` on a wide-address target, so byte representability is proved separately
///          before either the stack array or the typed dynamic allocator is instantiated.
template <::std::integral ch_type, typename... Args>
inline constexpr bool concat_retained_reserve_scatters_run_v = []() consteval {
	if constexpr (sizeof...(Args) != 0u &&
				  (::fast_io::details::decay::concat_retained_reserve_scatters_printable_v<
					   ch_type, Args> &&
				   ...))
	{
		using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
		constexpr auto capacity{
			::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
		return capacity.scatters_size != SIZE_MAX && capacity.reserve_size != SIZE_MAX &&
			   capacity.scatters_size <= SIZE_MAX / sizeof(scatter_type) &&
			   capacity.reserve_size <= SIZE_MAX / sizeof(ch_type);
	}
	else
	{
		return false;
	}
}();

/// @brief Proves that both exact plan arrays fit in concat's bounded hot stack frame.
/// @details Four KiB is a cost ceiling, while the configurable print stack policy is a safety ceiling; the smaller
///          value controls this plan.  Division precedes multiplication, proving the byte calculation cannot wrap.
///          A zero reserve capacity still needs one sentinel character because producers may compare their cursor.
template <::std::integral ch_type, typename... Args>
inline consteval bool concat_retained_reserve_scatters_stack_safe() noexcept
{
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	constexpr auto capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	constexpr ::std::size_t configured_budget{
		::fast_io::details::decay::print_stack_buffer_max_bytes()};
	constexpr ::std::size_t preferred_budget{4u * 1024u};
	constexpr ::std::size_t budget{
		configured_budget < preferred_budget ? configured_budget : preferred_budget};
	if constexpr (budget == 0u || capacity.scatters_size > budget / sizeof(scatter_type))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t scatter_bytes{capacity.scatters_size * sizeof(scatter_type)};
		constexpr ::std::size_t reserve_slots{
			capacity.reserve_size == 0u ? 1u : capacity.reserve_size};
		return reserve_slots <= (budget - scatter_bytes) / sizeof(ch_type);
	}
}

/// @brief Tests whether a producer cursor lies in the closed range of its declared component slice.
/// @details Equality with `last` is valid because reserve-scatters returns one-past cursors.  At run time integer
///          addresses avoid subtracting an untrusted out-of-range pointer; the stride test additionally rejects a
///          forged descriptor cursor that points into the middle of an array element.  Constant evaluation uses the
///          language's same-array pointer ordering, which is valid for every conforming customization.
template <typename element_type>
inline constexpr bool concat_reserve_scatters_cursor_in_closed_range(
	element_type *first, element_type *last, element_type *cursor) noexcept
{
	if (__builtin_is_constant_evaluated())
	{
		return first <= cursor && cursor <= last;
	}
	auto const first_address{reinterpret_cast<::std::uintptr_t>(first)};
	auto const last_address{reinterpret_cast<::std::uintptr_t>(last)};
	auto const cursor_address{reinterpret_cast<::std::uintptr_t>(cursor)};
	return first_address <= cursor_address && cursor_address <= last_address &&
		   (cursor_address - first_address) % sizeof(element_type) == 0u;
}

/// @brief Materializes one retained plan and copies its actual descriptor prefix into a string destination.
/// @details Each producer is invoked exactly once.  Its returned cursors are validated against that producer's own
///          capacity slice before they become the starting cursors for the next producer.  This induction proves that
///          later producers receive disjoint remaining storage and that cursor chaining stays inside the aggregate
///          arrays. Descriptor liveness through the final copy is a separate premise, supplied by
///          `concat_retained_reserve_scatters_printable_v` and its borrowed-source marker. Payload lengths and the
///          optional newline are then checked before one destination reserve.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized(
	T &str, ::fast_io::basic_io_scatter_t<ch_type> *scatters, ch_type *reserve, Args &...args)
{
	constexpr auto aggregate_capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	constexpr bool read_prfch{
		::fast_io::concat_scatter_chain_read_prfch_strategy<
			::fast_io::details::native_prfch_platform, aggregate_capacity.scatters_size, Args...>};
	auto scatter_curr{scatters};
	auto reserve_curr{reserve};
	auto materialize_one = [&scatter_curr, &reserve_curr](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		constexpr auto component_capacity{
			print_reserve_scatters_size(::fast_io::io_reserve_type<ch_type, arg_type>)};
		auto scatter_first{scatter_curr};
		auto reserve_first{reserve_curr};
		auto const result{print_reserve_scatters_define(
			::fast_io::io_reserve_type<ch_type, arg_type>, scatter_first, reserve_first, arg)};
		if (!::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
				scatter_first, scatter_first + component_capacity.scatters_size,
				result.scatters_pos_ptr) ||
			!::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
				reserve_first, reserve_first + component_capacity.reserve_size,
				result.reserve_pos_ptr)) [[unlikely]]
		{
			// A customization that reports a cursor outside its advertised slice has already broken the storage contract;
			// terminating here prevents that pointer from participating in subtraction, iteration, or a later CPO call.
			::fast_io::fast_terminate();
		}
		scatter_curr = result.scatters_pos_ptr;
		reserve_curr = result.reserve_pos_ptr;
	};
	(materialize_one(args), ...);

	bool runtime_read_prfch{};
	::std::size_t const payload_size{
		::fast_io::details::decay::calculate_scatter_total_size<read_prfch>(
			scatters, scatter_curr, runtime_read_prfch)};
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{
			strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, str, total_size);
	}
	auto iter{::fast_io::details::decay::copy_scatter_chain_to_buffer<read_prfch>(
		strlike_begin(::fast_io::io_strlike_type<ch_type, T>, str), scatters, scatter_curr,
		runtime_read_prfch)};
	if constexpr (line)
	{
		*iter++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	strlike_set_curr(::fast_io::io_strlike_type<ch_type, T>, str, iter);
}

/// @brief Owns the bounded descriptor/scratch storage for one retained reserve-scatters concat run.
/// @details Small plans use exact-size arrays whose combined frame is proven below four KiB.  Larger plans move both
///          arrays to isolated dynamic storage; zero reserve capacity uses a live one-character sentinel and performs
///          no pointless allocation.  In every case storage outlives descriptor summation and the destination copy.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_reserve_scatters(T &str, Args &...args)
{
	static_assert(
		::fast_io::details::decay::concat_retained_reserve_scatters_run_v<ch_type, Args...>);
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	constexpr auto capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	if constexpr (
		::fast_io::details::decay::concat_retained_reserve_scatters_stack_safe<ch_type, Args...>())
	{
		scatter_type scatters[capacity.scatters_size];
		if constexpr (capacity.reserve_size == 0u)
		{
			ch_type reserve{};
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters, __builtin_addressof(reserve), args...);
		}
		else
		{
			ch_type reserve[capacity.reserve_size];
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters, reserve, args...);
		}
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(capacity.scatters_size);
		if constexpr (capacity.reserve_size == 0u)
		{
			ch_type reserve{};
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters.ptr, __builtin_addressof(reserve), args...);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<ch_type> reserve(capacity.reserve_size);
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters.ptr, reserve.ptr, args...);
		}
	}
}

/// @brief Adds concat's optional newline while preserving a representable contiguous pointer range.
/// @details Precise-size CPOs are run-time customization points and may return any `size_t`.  Checked addition prevents
///          `SIZE_MAX + 1` from wrapping to an undersized allocation, while the `PTRDIFF_MAX` bound proves that both
///          `first + size` and the later begin/end difference are representable for one C++ array object.
template <bool line>
inline constexpr ::std::size_t concat_precise_size_with_line(::std::size_t precise_size)
{
	::std::size_t const total_size{::fast_io::details::intrinsics::add_or_overflow_die(
		precise_size, static_cast<::std::size_t>(line))};
	if (static_cast<::std::size_t>(PTRDIFF_MAX) < total_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return total_size;
}

/// @brief Caps the ordinary exact-resize plan's sizing table and per-call template expansion.
/// @details Each leaf needs one cached `size_t` so its exact size is not recomputed during emission. Sixteen leaves
///          keep that table at 128 bytes on a 64-bit ABI and cover the measured 2/4/8/12/16 composition shapes. The
///          optional newline does not consume a table entry: it contributes one checked character to the aggregate
///          size and one final store after every measured slice. An isolated GCC 15 x86-64 benchmark specialization
///          added only eight instructions at the sixteen-leaf line boundary, while paired run-time measurements kept
///          exact resize profitable for both line modes. A smaller line-only cap would therefore preserve more staging
///          work without removing a demonstrated per-leaf cost. Beyond sixteen leaves, the ordinary destination
///          dispatcher remains the bounded code-size policy: unrolling another sizing and checked-writer step per leaf
///          is not accepted without separate performance evidence. Semantic packs have their own shape-aware threshold
///          and do not use this flat-run limit.
inline constexpr ::std::size_t basic_general_concat_precise_resize_leaf_limit{16u};

/// @brief Identifies one leaf whose final concat extent is exactly known before destination construction.
/// @details A raw scatter deliberately does not model the public precise-reserve protocol: its borrowed source may
///          alias an arbitrary output sink, so globally promising a non-overlapping direct writer would be unsound.
///          Ordinary concat can be narrower when its destination explicitly opts into fresh-result disjointness. The
///          scatter length is then exact while its phase-one source remains alive. Keeping this exception local prevents
///          print and third-party destination dispatch from acquiring a stronger alias contract.
template <::std::integral char_type, typename Arg>
inline constexpr bool basic_general_concat_precise_resize_with_borrowed_scatter_component_v =
	::fast_io::precise_reserve_printable<char_type, Arg> ||
	::std::same_as<::std::remove_cvref_t<Arg>,
				   ::fast_io::basic_io_scatter_t<char_type>>;

/// @brief Selects the bounded all-precise strategy for an already-normalized ordinary leaf run.
/// @details This predicate proves only source shape and cost. Destination exact-resize capability is deliberately a
///          separate requirement on the implementation so an argument concept cannot accidentally imply writable
///          storage for a result type.
template <::std::integral char_type, typename... Args>
inline constexpr bool basic_general_concat_precise_resize_run_v = []() consteval {
	if constexpr (
		sizeof...(Args) == 0u ||
		::fast_io::details::decay::basic_general_concat_precise_resize_leaf_limit < sizeof...(Args))
	{
		// Rejecting the shape before forming the capability fold also bounds constraint-instantiation work for very
		// large packs; a disabled cost plan has no reason to probe every leaf's ADL surface.
		return false;
	}
	else
	{
		return (::fast_io::precise_reserve_printable<char_type, Args> && ...);
	}
}();

/// @brief Extends only the destination-gated exact-resize plan with borrowed raw scatter leaves.
/// @details The ordinary precise-run predicate above is also a staging-cost signal for initialization-sensitive
///          sources. Giving raw scatters that broader meaning would alter fallback strategy selection even when a
///          destination rejects borrowed-source resize. This sibling is therefore consumed solely by the destination
///          predicate below, where the independent alias/lifetime opt-in is checked in the same decision.
template <::std::integral char_type, typename... Args>
inline constexpr bool basic_general_concat_precise_resize_with_borrowed_scatter_run_v = []() consteval {
	if constexpr (
		sizeof...(Args) == 0u ||
		::fast_io::details::decay::basic_general_concat_precise_resize_leaf_limit < sizeof...(Args))
	{
		return false;
	}
	else
	{
		return (::fast_io::details::decay::
					basic_general_concat_precise_resize_with_borrowed_scatter_component_v<
						char_type, Args> &&
				...);
	}
}();

/// @brief Pairs an ordinary precise leaf run with the exact-resize cost of its final destination.
/// @details Source exactness and destination writable lifetime remain the base capability proof. A leaf may additionally
///          state that value-initializing the final extent is a measured extra pass; such a run is admitted only when
///          the destination explicitly creates that live extent without initialization. This is deliberately consumed
///          only by the flat ordinary strategy. Semantic concat can eliminate staging for width/condition/pack graphs
///          and therefore retains its independent whole-output decision. The source-shape test is performed first so a
///          pack rejected by the bounded leaf policy does not instantiate unrelated source or destination ADL probes.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_precise_resize_destination_run_v = []() consteval {
	if constexpr (!::fast_io::details::decay::
					  basic_general_concat_precise_resize_with_borrowed_scatter_run_v<
						  char_type, Args...>)
	{
		return false;
	}
	else if constexpr (
		((::std::same_as<::std::remove_cvref_t<Args>,
						 ::fast_io::basic_io_scatter_t<char_type>>) ||
		 ...) &&
		!::fast_io::concat_borrowed_scatter_precise_resize_safe_strlike<char_type, T>)
	{
		// A raw descriptor proves its length, not that an arbitrary destination resize is source-independent.
		return false;
	}
	else if constexpr (
		(::fast_io::precise_resize_initialization_sensitive_printable<char_type, Args> || ...))
	{
		return ::fast_io::precise_resize_without_initialization_strlike<char_type, T>;
	}
	else
	{
		return ::fast_io::precise_resize_writable_strlike<char_type, T>;
	}
}();

/// @brief Proves that an exact fresh-result run cannot fail after runtime spare storage has been exposed.
/// @details Every ordinary precise writer must return its endpoint and be non-failing through both the C++ exception
///          and Herbception channels. A raw scatter leaf is copied by fast_io's non-throwing primitive. Exact-size CPOs
///          may still fail, but concat evaluates and caches all of them before reserving the unpublished destination.
///          Consequently the runtime branch below never needs to destroy a string whose logical end was not published
///          after a source failure. This is deliberately weaker than the cached/replayable source proof used by the
///          speculative bounded strategy: each size CPO is invoked exactly once here.
template <::std::integral char_type, typename... Args>
inline constexpr bool basic_general_concat_runtime_precise_direct_source_run_v =
	((::std::same_as<::std::remove_cvref_t<Args>,
					 ::fast_io::basic_io_scatter_t<char_type>> ||
	  ::fast_io::nothrow_precise_reserve_printable<char_type, Args>) &&
	 ...);

/// @brief Selects an explicitly preferred exact construction for one fresh concat result.
/// @details This is intentionally a one-leaf, phase-one cost policy.  The source must advertise both the ordinary
///          precise protocol and its cheap-size preference.  A buffer string proves reserve/begin/commit directly;
///          a non-buffer string must satisfy concat's existing precise-resize destination proof.  No stream, existing
///          string, semantic graph, or unmarked precise producer inherits this choice from structural capabilities.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_fresh_precise_resize_preferred_run_v = []() consteval {
	// Reject multi-leaf and unmarked runs before probing any destination-specific exact-storage protocol.
	if constexpr (sizeof...(Args) != 1u ||
				  !(::fast_io::concat_fresh_precise_resize_printable_preferred<
						char_type, Args> &&
					...))
	{
		return false;
	}
	else
	{
		return ::fast_io::buffer_strlike<char_type, T> ||
			   ::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
				   char_type, T, Args...>;
	}
}();

/// @brief Admits one initialization-sensitive exact source to C++23 callback-owned string construction.
/// @details The ordinary exact-resize strategy permits a throwing formatter because its local result is destroyed if
///          emission fails. `std::basic_string::resize_and_overwrite` has a different callback boundary, so this
///          narrower strategy requires the exact named-lvalue define expression itself to be `noexcept` and to report
///          its actual endpoint. The initialization-sensitive marker is a cost proof: without it, the established
///          scatter/static-reserve priorities already avoid the extra destination-initialization pass this strategy is
///          designed to remove. Restricting the shape to one normalized leaf preserves the measured range-view case
///          without introducing a component-size table or an unbounded callback expansion.
template <typename char_type, typename Arg>
concept basic_general_concat_exact_overwrite_source =
	::std::integral<char_type> &&
	::fast_io::precise_resize_initialization_sensitive_printable<char_type, Arg> &&
	::fast_io::nothrow_precise_reserve_printable<char_type, Arg>;

/// @brief Writes one proved exact source into storage owned by a resize-and-overwrite destination.
/// @details `concat_precise_size_with_line` has already proved `total_size <= PTRDIFF_MAX`, and `payload_size` is no
///          greater than that total. Consequently the guarded endpoint addition stays inside the callback's writable
///          array. The source cursor is compared with that exact endpoint before either the newline or logical size is
///          published. The precise-reserve contract, rather than this post-call cursor comparison, proves that the
///          producer confines its writes to `[buffer, expected_end)`; the comparison validates only the endpoint it
///          reports and cannot retroactively detect an already out-of-bounds write by a broken customization. A short
///          callback extent is a broken destination contract and terminates before any write. The source concept proves
///          that the only user-extensible expression executed here cannot throw; pointer arithmetic, comparison, the
///          optional character store, and `fast_terminate` are non-throwing operations.
template <bool line, ::std::integral char_type, typename Arg>
	requires ::fast_io::nothrow_precise_reserve_printable<char_type, Arg>
struct basic_general_concat_exact_overwrite_operation
{
	Arg *argument;
	::std::size_t payload_size;
	::std::size_t total_size;

	inline constexpr ::std::size_t operator()(char_type *buffer, ::std::size_t writable_size) const noexcept
	{
		// Keeping both extents is intentional even though it makes this state three words on LP64. A two-word variant was
		// motivated by the SysV AMD64/AAPCS64 small-aggregate register classes, not by a cross-ABI theorem: MS x64,
		// RISC-V, PPC64, and ILP32 targets classify aggregates differently. In the measured GCC 15/libstdc++ build the
		// complete callback was inlined, so no by-value ABI transfer remained; reconstruction instead enlarged the hot line
		// specialization and regressed the 128-element E-core case by about 6.7 percent. Passing the already checked values
		// avoids that optimizer cliff. Aggregate-size decay heuristics were therefore not predictive for this measured
		// inlined specialization; other ABI/library/compiler combinations remain governed by their own code generation.
		if (writable_size < total_size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		char_type *expected_end{buffer};
		if (payload_size != 0u)
		{
			expected_end += payload_size;
		}
		using arg_type = ::std::remove_cvref_t<Arg>;
		char_type *const actual_end{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, buffer, payload_size, *argument)};
		if (actual_end != expected_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if constexpr (line)
		{
			*expected_end = ::fast_io::char_literal_v<u8'\n', char_type>;
		}
		return total_size;
	}
};

/// @brief Admits a cached-size fresh concat source to callback-owned exact construction.
/// @details Unlike the older initialization-sensitive range policy below, this proof is independent of `line`: the
///          source has explicitly selected exact fresh construction and its size query is cheap, while the nothrow
///          precise writer satisfies the callback's exception boundary.  The concrete destination operation remains
///          part of the predicate, so a marker cannot manufacture resize-and-overwrite storage structurally.
template <bool line, ::std::integral char_type, typename T, typename Arg>
inline constexpr bool basic_general_concat_fresh_precise_exact_overwrite_run_v = []() constexpr {
	// A callback-owned overwrite is valid only when sizing is preferred and the exact writer cannot escape by throwing.
	if constexpr (
		!::fast_io::concat_fresh_precise_resize_printable_preferred<char_type, Arg> ||
		!::fast_io::nothrow_precise_reserve_printable<char_type, Arg>)
	{
		return false;
	}
	else
	{
		using operation =
			::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
				line, char_type, Arg>;
		return ::fast_io::exact_resize_and_overwrite_strlike_for<
			char_type, T, operation>;
	}
}();

/// @brief Pairs the exact source operation with the concrete destination callback CPO.
/// @details Testing the final operation type, rather than only the destination marker, closes the strategy/body gap:
///          a destination cannot opt in with a marker while rejecting the callback concat actually passes.
template <bool line, ::std::integral char_type, typename T, typename Arg>
inline constexpr bool basic_general_concat_exact_overwrite_run_v = []() constexpr {
	if constexpr (!line)
	{
		// Without a newline, the retained-scatter fallback traverses this range once into concat's 2-KiB inline staging
		// area and lets basic_string bulk-copy the completed bytes. Exact overwrite replaces that bulk copy with a full
		// sizing traversal. Formal paired P-core measurements found a 1.33-percent regression at 128 elements,
		// while E-core results moved in the opposite direction; there is therefore no portable cost proof for selecting
		// the callback path. The line case has independent evidence because it also fuses newline publication and avoids
		// the substantially larger legacy line helper. Keep this policy in the concept-level predicate so every caller
		// observes the same admission decision rather than duplicating a platform heuristic in the dispatch body.
		return false;
	}
	else if constexpr (!::fast_io::details::decay::basic_general_concat_exact_overwrite_source<
						   char_type, Arg>)
	{
		return false;
	}
	else
	{
		using operation = ::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
			line, char_type, Arg>;
		return ::fast_io::exact_resize_and_overwrite_strlike_for<char_type, T, operation>;
	}
}();

/// @brief Constructs the final string with one sizing pass and one direct overwrite pass.
/// @details Sizing remains outside the callback and may throw normally. Only after a representable final extent is
///          known is the local destination constructed and asked to allocate. The callback then writes directly into
///          that allocation, avoiding both value-initialization of the final range and concat's descriptor/staging
///          string. If allocation throws, the local result owns no published partial value; after callback entry, the
///          source and endpoint proofs above make the operation non-throwing.
template <bool line, ::std::integral char_type, typename T, typename Arg>
	requires(
		::fast_io::nothrow_precise_reserve_printable<char_type, Arg> &&
		::fast_io::exact_resize_and_overwrite_strlike_for<
			char_type, T,
			::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
				line, char_type, Arg>>)
inline constexpr T basic_general_concat_exact_overwrite_construct(Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const payload_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, arg_type>, arg)};
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	using operation = ::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
		line, char_type, Arg>;
	operation overwrite{__builtin_addressof(arg), payload_size, total_size};
	T result;
	strlike_exact_resize_and_overwrite(
		::fast_io::io_strlike_type<char_type, T>, result, total_size, overwrite);
	return result;
}

/// @brief Reuses the measured exact-overwrite policy gate while delegating construction to the shared operation helper.
template <bool line, ::std::integral char_type, typename T, typename Arg>
	requires ::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
		line, char_type, T, Arg>
inline constexpr T basic_general_concat_exact_overwrite_run(Arg &arg)
{
	return ::fast_io::details::decay::basic_general_concat_exact_overwrite_construct<
		line, char_type, T>(arg);
}

template <::std::integral char_type, typename Arg>
inline constexpr void
basic_general_concat_precise_resize_measure_one(
	::std::size_t *component_sizes, ::std::size_t &payload_size,
	::std::size_t &component_index, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const component_size{[](Arg &value) constexpr {
		if constexpr (::std::same_as<arg_type,
									 ::fast_io::basic_io_scatter_t<char_type>>)
		{
			return value.len;
		}
		else
		{
			return print_reserve_precise_size(
				::fast_io::io_reserve_type<char_type, arg_type>, value);
		}
	}(arg)};
	component_sizes[component_index++] = component_size;
	payload_size = ::fast_io::details::intrinsics::add_or_overflow_die(
		payload_size, component_size);
}

template <::std::integral char_type, typename Arg>
inline constexpr void
basic_general_concat_precise_resize_emit_one(
	::std::size_t const *component_sizes, ::std::size_t &component_index,
	char_type *&current, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const component_size{component_sizes[component_index++]};
	char_type *expected_end{current};
	if (component_size != 0u)
	{
		expected_end += component_size;
	}
	if constexpr (::std::same_as<arg_type,
								 ::fast_io::basic_io_scatter_t<char_type>>)
	{
		// Zero-length scatters may legally carry a null base. For non-empty scatters the newly constructed result owns
		// disjoint storage, so the faster non-overlap primitive is valid without strengthening raw scatter's CPOs.
		if (component_size != 0u)
		{
			::fast_io::details::non_overlapped_copy_n(
				arg.base, component_size, current);
		}
	}
	else
	{
		using define_result = decltype(print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, current,
			component_size, arg));
		if constexpr (::std::same_as<define_result, char_type *>)
		{
			char_type *const actual_end{print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, arg_type>, current,
				component_size, arg)};
			if (actual_end != expected_end) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
		else
		{
			print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, arg_type>, current,
				component_size, arg);
		}
	}
	current = expected_end;
}

/// @brief Constructs a non-buffer string-like result directly in one exactly resized logical range.
/// @details Exact source sizing and destination storage are orthogonal proofs: every
///          `precise_reserve_printable` supplies one component extent, while `precise_resize_writable_strlike` creates
///          live characters without pretending that spare capacity is a put area. Arguments are accepted by reference
///          because phase 1 already owns their decayed transports; copying the graph again could change an identity-
///          sensitive formatter or retain a reference to a temporary proxy.
///
///          Measurement is left-to-right. Each extent is cached and added with checked arithmetic before resize, and
///          `concat_precise_size_with_line` proves the complete allocation is a representable C++ array range. Emission
///          is then left-to-right into disjoint adjacent slices. A pointer-returning writer is checked against its own
///          promised slice end before the next writer runs; a void writer advances only by the exact protocol contract.
///          This induction proves that no writer can redirect a later writer or make concat publish an incorrect end.
///          The strategy invokes no scatter producer and therefore needs no descriptor-retention or borrowed-lifetime
///          assumption.
///
///          On an audited runtime put-area destination, a completely non-failing writer run reserves the exact capacity
///          and publishes one final cursor, avoiding the value-initialization pass of portable resize. All size CPOs are
///          cached before that reserve, and the source predicate proves that no later user operation can fail. Other
///          runs retain exact resize: a sizing exception occurs before resize, while a resize or writer exception leaves
///          every live character owned by the local result. C++23 `resize_and_overwrite` is not used because an
///          arbitrary formatting CPO is permitted to throw.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
		char_type, T, Args...>
inline constexpr T
basic_general_concat_precise_resize_run(Args &...args)
{
	T result;
	::std::size_t component_sizes[sizeof...(Args)]{};
	::std::size_t payload_size{};
	::std::size_t component_index{};
	(::fast_io::details::decay::basic_general_concat_precise_resize_measure_one<
		 char_type>(component_sizes, payload_size, component_index, args),
	 ...);
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	FAST_IO_IF_NOT_CONSTEVAL
	{
		if constexpr (
			::fast_io::concat_fresh_runtime_exact_direct_strlike<char_type, T> &&
			::fast_io::details::decay::basic_general_concat_runtime_precise_direct_source_run_v<
				char_type, Args...>)
		{
			strlike_runtime_reserve(
				::fast_io::io_strlike_type<char_type, T>, result, total_size);
			char_type *const first{
				strlike_runtime_curr(::fast_io::io_strlike_type<char_type, T>, result)};
			char_type *const last{
				strlike_runtime_end(::fast_io::io_strlike_type<char_type, T>, result)};
			if (first == last)
			{
				if (total_size != 0u) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
			}
			else if (first == nullptr || last == nullptr) [[unlikely]]
			{
				// Equality is handled before subtraction so a conforming zero-capacity destination may use null cursors.
				::fast_io::fast_terminate();
			}
			else
			{
				::std::ptrdiff_t const available{last - first};
				if (available < 0 || total_size > static_cast<::std::size_t>(available)) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
			}

			component_index = 0u;
			char_type *current{first};
			(::fast_io::details::decay::basic_general_concat_precise_resize_emit_one<
				 char_type>(component_sizes, component_index, current, args),
			 ...);
			if constexpr (line)
			{
				*current++ = ::fast_io::char_literal_v<u8'\n', char_type>;
			}
			char_type *const expected_end{
				total_size == 0u ? first : first + total_size};
			if (current != expected_end) [[unlikely]]
			{
				// Per-component endpoint checks and the checked size fold should make this aggregate invariant tautological.
				::fast_io::fast_terminate();
			}
			strlike_runtime_set_curr(
				::fast_io::io_strlike_type<char_type, T>, result, current);
			return result;
		}
	}
	char_type *const first{strlike_precise_resize_and_get_begin(
		::fast_io::io_strlike_type<char_type, T>, result, total_size)};

	component_index = 0u;
	char_type *current{first};
	(::fast_io::details::decay::basic_general_concat_precise_resize_emit_one<
		 char_type>(component_sizes, component_index, current, args),
	 ...);
	if constexpr (line)
	{
		*current = ::fast_io::char_literal_v<u8'\n', char_type>;
	}
	return result;
}

/// @brief Emits one precisely sized leaf into a fresh writable result and publishes only its verified endpoint.
template <bool line, ::std::integral ch_type, typename T, typename Arg>
inline constexpr void basic_general_concat_decay_ref_impl_precise(T &str, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t precise_size{
		print_reserve_precise_size(io_reserve_type<ch_type, arg_type>, arg)};
	::std::size_t const precise_size_with_line{
		::fast_io::details::decay::concat_precise_size_with_line<line>(precise_size)};
	// SSO destinations need growth only when the exact record exceeds their implementation-provided local capacity.
	if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{
			strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < precise_size_with_line)
		{
			strlike_reserve(
				io_strlike_type<ch_type, T>, str, precise_size_with_line);
		}
	}
	else
	{
		strlike_reserve(
			io_strlike_type<ch_type, T>, str, precise_size_with_line);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{first};
	if (precise_size != 0u)
	{
		ptr += precise_size;
	}
	using define_result = decltype(print_reserve_precise_define(
		io_reserve_type<ch_type, arg_type>, first, precise_size, arg));
	// Pointer-reporting writers permit an endpoint check; void writers retain their exact-extent protocol unchanged.
	if constexpr (::std::same_as<define_result, ch_type *>)
	{
		ch_type *const actual_end{print_reserve_precise_define(
			io_reserve_type<ch_type, arg_type>, first, precise_size, arg)};
		if (actual_end != ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	else
	{
		print_reserve_precise_define(
			io_reserve_type<ch_type, arg_type>, first, precise_size, arg);
	}
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief Materializes one source-preferred exact leaf into a fresh result.
/// @details Buffer strings keep their logical cursor unpublished until exact emission succeeds, then commit the
///          returned range once.  Other destinations use the precise-resize protocol, whose local result likewise
///          contains every partially constructed state if allocation or formatting throws.  Both branches delegate to
///          the source's precise writer; this strategy never lowers the operation to an opaque `print_define` call.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_fresh_precise_resize_preferred_run_v<
		char_type, T, Args...>
inline constexpr T
basic_general_concat_fresh_precise_resize_preferred_run(Args &...args)
{
	// Writable buffer results can reserve and publish the exact source directly through their cursor protocol.
	if constexpr (::fast_io::buffer_strlike<char_type, T>)
	{
		T result;
		(::fast_io::details::decay::basic_general_concat_decay_ref_impl_precise<
			 line, char_type, T>(result, args),
		 ...);
		return result;
	}
	// Non-buffer results use callback-owned storage only when the concrete operation satisfies its exact CPO.
	else if constexpr (
		(::fast_io::details::decay::basic_general_concat_fresh_precise_exact_overwrite_run_v<
			 line, char_type, T, Args> &&
		 ...))
	{
		// The source has a cheap cached size, and the destination can expose uninitialized callback-owned storage.
		// This avoids std::string::resize value-initializing the complete document immediately before it is overwritten.
		return (::fast_io::details::decay::basic_general_concat_exact_overwrite_construct<
					line, char_type, T>(args),
				...);
	}
	else
	{
		return ::fast_io::details::decay::basic_general_concat_precise_resize_run<
			line, char_type, T>(args...);
	}
}

/// @brief  Maximum semantic leaf count for which concat performs a separate exact-size traversal.
/// @details Benchmarked long packs are faster when a static reserve bound drives one-pass materialization. The strict
///          threshold keeps compact packs on exact allocation while routing eight-leaf and larger packs to bounded
///          generation, whose returned cursor supplies the actual constructed length.
inline constexpr ::std::size_t basic_general_concat_precise_leaf_threshold{
	::fast_io::details::decay::print_semantic_precise_materialization_leaf_threshold};

/// @brief    Counts the maximum semantic leaves in a concat argument list.
/// @tparam   Args the concat argument types
template <typename... Args>
inline constexpr ::std::size_t basic_general_concat_semantic_leaf_count =
	::fast_io::details::decay::print_semantic_leaf_count_sum<Args...>();

/// @brief    Tests whether every concat argument supplies a compile-time semantic upper bound.
/// @tparam   ch_type the concat character type
/// @tparam   Args    the concat argument types
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_static_bounded =
	(::fast_io::details::decay::print_semantic_static_bounded_size<ch_type, ::std::remove_cvref_t<Args>>::available &&
	 ...);

/// @brief    Selects exact-size semantic concat for compact compositions that can consume precise sizing.
/// @details  Long statically bounded packs deliberately bypass this path so bounded materialization can generate each
///           integer once. Compact conditions, width packs, and mixed packs retain exact allocation when it reduces
///           allocation size or output operations.
/// @tparam   ch_type the concat character type
/// @tparam   Args    the concat argument types
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_precise_ok =
	(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::details::decay::print_semantic_precise_size_ok<ch_type, Args &>::value && ...) &&
	(::fast_io::details::decay::basic_general_concat_semantic_leaf_count<Args...> <
		 ::fast_io::details::decay::basic_general_concat_precise_leaf_threshold ||
	 !::fast_io::details::decay::basic_general_concat_semantic_static_bounded<ch_type, Args...>);

/// @brief Selects one-pass upper-bound materialization for semantic runs that are not precisely measurable.
/// @details A width node around an ordinary reserve formatter has an object-dependent field extent but no precise
///          child protocol.  Treating that node as a generic destination fallback loses the semantic boundary on
///          some string adapters.  The bounded semantic engine already proves both capacity and returned-cursor
///          correctness, so concat can reserve the run-time bound once and construct exactly the produced prefix.
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_bounded_ok =
	(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::details::decay::print_semantic_bounded_size_ok<ch_type, Args &>::value && ...);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl(T &str, Args &...args);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_impl(Args... args);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_ref_impl(Args &...args);

/// @brief Proves that the normalized fallback run can write directly into the result object's string adapter.
/// @details A construct-only string-like type has no `io_strlike_ref`, while append-oriented and writable-buffer
///          results do. Keeping that distinction in a SFINAE-friendly value lets non-buffer results avoid an internal
///          materialize-and-copy cycle when their real adapter accepts the complete run. `io_strlike_ref` is already
///          the string protocol's normalized output reference, so the proof deliberately tests that exact unprojected
///          type used by the body; reopening `output_stream_ref` here would admit an adapter object which the body never
///          invokes. Character-domain equality is independent evidence: a writable associated ref for another domain
///          cannot hide the maintained generic adapter for `ch_type`.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_direct_destination_ok = []() constexpr {
	// Probe an associated result adapter only when its ADL customization is actually well formed.
	if constexpr (requires(T &str) { io_strlike_ref(::fast_io::io_alias, str); })
	{
		using destination_reference = decltype(io_strlike_ref(::fast_io::io_alias, ::std::declval<T &>()));
		using destination_type = ::std::remove_reference_t<destination_reference>;
		using destination_object_type = ::std::remove_cv_t<destination_type>;
		// An explicit character domain is required before the adapter may receive this normalized print run.
		if constexpr (requires { typename destination_object_type::output_char_type; })
		{
			return ::std::same_as<ch_type, typename destination_object_type::output_char_type> &&
				   ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
					   line, destination_type, Args...>;
		}
		else
		{
			// Naming `io_strlike_ref` alone is not a complete output capability. Keep a malformed or unrelated associated
			// CPO inside the false branch so an independently valid generic writable-buffer adapter remains selectable.
			return false;
		}
	}
	else
	{
		return false;
	}
}();

/// @brief Selects direct fresh-result emission for exactly one normalized fixed decimal scalar.
/// @details Let `S` be the normalized scalar leaf and `D` the default-constructed result.  The source trait proves that
///          `S` emits one nonempty built-in decimal spelling with no width, sign-display, radix, punctuation, or other
///          semantic policy.  The destination marker independently promises both the cost preference and observable
///          equivalence between the fresh adapter's complete dispatcher and reserve staging followed by range
///          construction; `basic_general_concat_direct_destination_ok` then proves that exact line-aware dispatcher is
///          callable on `D`'s actual adapter.  The implementation additionally ends the adapter lifetime before moving
///          `D`, so destructor-based publication is complete.  The one-leaf test is performed before either ADL
///          destination probe so unrelated packs do not instantiate or inherit this narrow construction policy.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_fresh_fixed_scalar_direct_run_v = []() constexpr {
	if constexpr (
		sizeof...(Args) != 1u ||
		!(::fast_io::details::decay::print_concat_fresh_fixed_decimal_scalar_traits<
			  ::std::remove_cvref_t<Args>>::available &&
		  ...))
	{
		return false;
	}
	else
	{
		return ::fast_io::concat_fresh_fixed_scalar_direct_preferred_strlike<
				   ch_type, T> &&
			   ::fast_io::details::decay::basic_general_concat_direct_destination_ok<
				   line, ch_type, T, Args...>;
	}
}();

/// @brief Proves the complete print protocol for a source-preferred one-pass run against the fresh result's adapter.
/// @details The one-pass source marker is a cost proof, not permission to skip destination semantics.  In particular,
///          an associated string adapter may own a status customization or mutex protocol which must remain outside
///          its ordinary direct-print CPO.  Admission therefore mirrors the complete freestanding dispatcher rather
///          than merely checking that each leaf has a callable `print_define`.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_one_pass_direct_destination_ok = []() constexpr {
	// A source cost preference cannot manufacture a destination adapter, so admit only a valid associated CPO.
	if constexpr (requires(T &str) { io_strlike_ref(::fast_io::io_alias, str); })
	{
		using destination_reference = decltype(io_strlike_ref(::fast_io::io_alias, ::std::declval<T &>()));
		using destination_type = ::std::remove_reference_t<destination_reference>;
		using destination_object_type = ::std::remove_cv_t<destination_type>;
		// Keep the one-pass producer in the requested character domain before invoking the full print protocol.
		if constexpr (requires { typename destination_object_type::output_char_type; })
		{
			return ::std::same_as<ch_type, typename destination_object_type::output_char_type> &&
				   ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
					   line, destination_type, Args...>;
		}
	}
	return false;
}();

/// @brief Selects a fresh append-oriented result for a source-proved one-pass run.
/// @details This is deliberately a construct-only phase-one cost policy.  It neither changes concat-to-existing-string
///          dispatch nor teaches an arbitrary stream that incremental writes are cheap.  Every normalized leaf must
///          explicitly prefer its direct one-pass representation, and the exact fresh result adapter must accept the
///          complete run.  The source marker is the explicit evidence that even a writable result should grow while
///          consuming this producer once instead of replaying it for an exact/dynamic bound.  Semantic nodes retain
///          their own whole-graph sizing/emission rules rather than acquiring a flat-leaf cost policy from one child
///          marker.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_one_pass_direct_destination_run_v =
	!(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::concat_one_pass_printable_preferred<ch_type, Args> && ...) &&
	::fast_io::details::decay::basic_general_concat_one_pass_direct_destination_ok<
		line, ch_type, T, Args...>;

/// @brief Emits one source-preferred run through the destination's complete print protocol.
/// @details The normalized string adapter supplies an ordinary buffered/put-area cost proof, so the print dispatcher
///          still selects the producer's direct one-pass representation without a dynamic-size replay.  Entering that
///          dispatcher is nevertheless essential: it acquires a complete output mutex before observing any status CPO,
///          and delegates a whole-run status customization before selecting the underlying buffer strategy.  Thus the
///          concat cost policy changes materialization only; it does not demote the destination to a bare `print_define`
///          sink.
template <bool line, ::std::integral ch_type, typename output_type, typename... Args>
inline constexpr void basic_general_concat_one_pass_direct_emit(
	output_type &output, Args &...args)
{
	::fast_io::operations::decay::print_freestanding_decay_impl<line>(output, args...);
}

/// @brief Proves a fallback run against fast_io's maintained adapter for an exact writable-buffer result.
/// @details `buffer_strlike` already supplies the cursor protocol needed by `io_strlike_reference_wrapper`; requiring a
///          separate user `io_strlike_ref` adds no semantic evidence and rejects otherwise complete buffer-only result
///          types. This predicate is deliberately separate from the custom-adapter proof above because an associated
///          adapter may have destination-specific direct CPOs which the generic cursor adapter neither gains nor hides.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_generic_buffer_destination_ok = []() constexpr {
	if constexpr (::fast_io::buffer_strlike<ch_type, T>)
	{
		using destination_type = ::fast_io::io_strlike_reference_wrapper<ch_type, T>;
		using destination_reference = decltype(::fast_io::operations::output_stream_ref(::std::declval<destination_type>()));
		return ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			line, destination_reference, Args...>;
	}
	else
	{
		return false;
	}
}();

/// @brief Requires one contiguous staging construction when exact resize would duplicate an initialization pass.
/// @details An initialization-sensitive ordinary precise run has already proved that every output character can be
///          measured before emission. If the result cannot establish that exact live range without first writing it,
///          the exact-resize plan correctly declines. Falling through to an append adapter would nevertheless turn the
///          same producer into one destination write per fragment (for example, every range element and separator),
///          which loses the reason for measuring the run and was substantially slower for long range views. Staging
///          instead performs the established measure/write passes into one contiguous growable buffer and constructs
///          the result once from the completed range. This predicate is a profitability rule only: the direct and
///          staging predicates below remain the capability and lifetime proofs. The ordinary-run test is deliberately
///          evaluated first, both to preserve unrelated direct-output behavior and to avoid probing marker CPOs for a
///          source shape that the bounded exact strategy has already rejected.
template <::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_initialization_sensitive_staging_required_v = []() consteval {
	if constexpr (!::fast_io::details::decay::basic_general_concat_precise_resize_run_v<
					  ch_type, Args...>)
	{
		return false;
	}
	else
	{
		return (::fast_io::precise_resize_initialization_sensitive_printable<ch_type, Args> || ...) &&
			   !::fast_io::precise_resize_without_initialization_strlike<ch_type, T>;
	}
}();

/// @brief Proves the alternate construct-only fallback against concat's actual staging stream.
/// @details Non-buffer results that cannot accept direct output are formed from an internal `basic_concat_buffer`.
///          This is a distinct destination from the final string adapter; treating the two as interchangeable admits
///          destination-specific `print_define` overloads that fail only after phase selection. The paired direct and
///          staging predicates are therefore the complete destination-disjunction for non-buffer concat fallback.
template <bool line, ::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_staging_destination_ok = []() constexpr {
	using staging_type = ::fast_io::details::basic_concat_buffer<ch_type>;
	using destination_type = decltype(io_strlike_ref(::fast_io::io_alias, ::std::declval<staging_type &>()));
	using destination_reference = decltype(::fast_io::operations::output_stream_ref(::std::declval<destination_type>()));
	return ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
		line, destination_reference, Args...>;
}();

/// @brief Recognizes an explicit destination cost-policy opt-in for single-pass context staging.
/// @details `print_context_static_buffer_size` bounds one refill window, not the complete output, so it cannot prove an
///          exact destination reserve.  A destination may instead return `std::true_type` from
///          `concat_context_staging_preferred(io_strlike_type<ch_type, T>)` when formatting once into concat's inline
///          buffer and then range-constructing `T` is cheaper than growing `T` directly.  Exact return-type matching
///          makes this an affirmative policy decision; structural string capabilities alone never enable the copy.
template <typename ch_type, typename T>
concept basic_general_concat_context_staging_preferred_destination =
	::std::integral<ch_type> && ::fast_io::range_constructible_strlike<ch_type, T> && requires {
		{
			concat_context_staging_preferred(::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Recognizes an audited fresh result whose long destination-neutral fallback should use ordered staging.
/// @details The returned nonzero count is a profitability threshold in normalized leaves, not a semantic capability.
///          For an admitted run the destination promises that range construction from concat's completed staging
///          interval has the same character result as its ordinary fresh output adapter and is cheaper from this count
///          onward. Requiring an exact constant-expression result makes the decision local to a concrete string
///          integration; range construction or writable-buffer syntax alone never opts a user type into an extra copy.
template <typename ch_type, typename T>
concept basic_general_concat_ordered_staging_preferred_destination =
	::std::integral<ch_type> && ::fast_io::range_constructible_strlike<ch_type, T> && requires {
		typename ::std::integral_constant<
			::std::size_t,
			concat_ordered_staging_minimum_leaf_count(
				::fast_io::io_strlike_type<ch_type, T>)>;
		{
			concat_ordered_staging_minimum_leaf_count(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::size_t>;
		requires(concat_ordered_staging_minimum_leaf_count(
				 ::fast_io::io_strlike_type<ch_type, T>) != 0u);
	};

/// @brief Recognizes a destination whose ordered-staging cost proof covers every long neutral-protocol pack.
/// @details The ordinary threshold marker is intentionally insufficient: most destinations have evidence only when an
///          unretained scatter forces immediate consumption and prevents concat's grouped planner. This independent,
///          exact-`true_type` CPO lets one audited destination state the stronger cost result that staging also beats its
///          exact-resize or retained-mixed strategy when every leaf is otherwise destination-neutral. It grants no
///          source capability and cannot admit a direct, context, or scatter-list customization.
template <typename ch_type, typename T>
concept basic_general_concat_ordered_staging_complete_neutral_preferred_destination =
	::std::integral<ch_type> && requires {
		{
			concat_ordered_staging_complete_neutral_preferred(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Recognizes a fresh result which may replace a bounded ordered staging area after it fills.
/// @details The marker is a semantic opt-in, not a consequence of writable-buffer syntax. It promises that default
///          construction creates result-owned storage disjoint from every source object, reserve preserves the already
///          copied prefix without consulting a source, and ordinary writes followed by cursor publication produce the
///          same value as range construction from that prefix. It also explicitly accepts the direct-output exception
///          boundary: allocation failure during promotion may prevent later source CPOs from running, whereas a fully
///          staged construction would allocate only after all producers completed. No partial result escapes either
///          path. The marker also promises that output hooks associated with this private adapter cannot observe the
///          temporarily unpublished logical end. Deferred-commit safety for the actually selected ordinary or runtime
///          cursor family then proves that cached cursors may advance without publishing after every leaf. These facts
///          let concat switch destinations between two source CPO invocations, or inside the overflow handling of one
///          invocation, without replaying that source.
template <typename ch_type, typename T>
concept basic_general_concat_ordered_staging_adaptive_destination =
	::std::integral<ch_type> && ::fast_io::output_buffer_strlike<ch_type, T> &&
	::std::constructible_from<T> && ::std::constructible_from<T, T &&> &&
	::std::is_nothrow_default_constructible_v<T> &&
	::std::is_nothrow_move_constructible_v<T> &&
	::std::is_nothrow_destructible_v<T> &&
	((::fast_io::buffer_strlike<ch_type, T> &&
	  ::fast_io::deferred_obuffer_commit_safe_strlike<ch_type, T>) ||
	 (!::fast_io::buffer_strlike<ch_type, T> &&
	  ::fast_io::runtime_deferred_obuffer_commit_safe_strlike<ch_type, T>)) &&
	::fast_io::details::output_buffer_strlike_begin_nothrow<ch_type, T>() &&
	::fast_io::details::output_buffer_strlike_curr_nothrow<ch_type, T>() &&
	::fast_io::details::output_buffer_strlike_end_nothrow<ch_type, T>() &&
	::fast_io::details::output_buffer_strlike_set_curr_nothrow<ch_type, T>() &&
	requires {
		{
			concat_ordered_staging_adaptive_promotion_safe(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Proves that changing only the fresh destination cannot change a fallback leaf's selected source protocol.
/// @details Static reserve, dynamic reserve, and scatter CPOs receive only the normalized source and caller-owned
///          contiguous storage; they cannot dispatch on the final string adapter. A direct/context or scatter-list
///          customization can observe a different output type or carry a multi-object protocol, so such a leaf is
///          excluded even when it also happens to expose one of the three contiguous protocols. The exact named-lvalue
///          scatter expression matches phase one's owned decay objects. This is a source-equivalence proof only;
///          immediate scatter consumption and CPO ordering remain the responsibility of either the ordinary dispatcher
///          or the protocol-equivalent adaptive one-shot emitter selected below.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_ordered_staging_leaf_v =
	(::fast_io::reserve_printable<ch_type, T> ||
	 ::fast_io::dynamic_reserve_printable<ch_type, T> ||
	 ::fast_io::scatter_printable_for<ch_type, T &>) &&
	!::fast_io::printable<ch_type, T> &&
	!::fast_io::context_printable<ch_type, T> &&
	!::fast_io::reserve_scatters_printable<ch_type, T> &&
	!::fast_io::dynamic_reserve_scatters_printable<ch_type, T>;

/// @brief Identifies an ordered leaf whose ordinary contiguous protocol has one destination-independent spelling.
/// @details Let `R`, `D`, and `S` denote static-reserve, dynamic-reserve, and named-lvalue scatter availability.
///          `print_control_single` selects `R` before `D` whenever `S` is absent, so `R && D && !S` may share the
///          static-reserve spelling. A source exposing `S` together with either reserve protocol is deliberately
///          rejected: ordinary scatter priority also depends on the static-scatter representation and destination
///          retention policy, neither of which follows from these three shape concepts. The surrounding staging-leaf
///          proof continues to exclude direct, context, and scatter-list CPOs. Consequently this predicate authorizes
///          only `(R && !S) || (!R && D && !S) || (!R && !D && S)`, exactly the cases for which a local one-shot emitter
///          can remove generic dispatcher state without changing the selected source customization.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_ordered_one_shot_leaf_v =
	::fast_io::details::decay::basic_general_concat_ordered_staging_leaf_v<
		ch_type, T> &&
	((::fast_io::reserve_printable<ch_type, T> &&
	  !::fast_io::scatter_printable_for<ch_type, T &>) ||
	 (!::fast_io::reserve_printable<ch_type, T> &&
	  ::fast_io::dynamic_reserve_printable<ch_type, T> &&
	  !::fast_io::scatter_printable_for<ch_type, T &>) ||
	 (!::fast_io::reserve_printable<ch_type, T> &&
	  !::fast_io::dynamic_reserve_printable<ch_type, T> &&
	  ::fast_io::scatter_printable_for<ch_type, T &>));

/// @brief Forms the type-and-cost candidate before the concrete staging output's interception checks are available.
/// @details Every normalized leaf is emitted once, in fold order, through the ordinary single-leaf dispatcher or the
///          adaptive destination's protocol-equivalent one-shot overload. Consequently an unmarked scatter descriptor
///          is copied before the next producer is invoked; no descriptor, size result, or formatter object is retained
///          by this policy. An unretained scatter is the ordinary profitability premise. A destination may independently
///          publish the stronger complete-neutral cost CPO when measurements also justify replacing homogeneous
///          precise/dynamic and fully retained mixed plans. The explicit destination threshold bounds staging setup to
///          pack sizes measured to amortize it; an independently marked runtime destination may cap the retained local
///          prefix at 512 characters and continue in the final result after one monotonic promotion.
template <::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_ordered_staging_candidate_v = []() consteval {
	if constexpr (
		!::fast_io::details::decay::basic_general_concat_ordered_staging_preferred_destination<
			ch_type, T> ||
		!(::fast_io::details::decay::basic_general_concat_ordered_staging_leaf_v<
			  ch_type, Args> &&
		  ...))
	{
		return false;
	}
	else if constexpr (
		::fast_io::concat_fresh_runtime_exact_direct_strlike<ch_type, T> &&
		::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
			ch_type, T, Args...> &&
		::fast_io::details::decay::basic_general_concat_runtime_precise_direct_source_run_v<
			ch_type, Args...>)
	{
		// A cached-once exact run can reserve and write the final runtime put area directly. That strictly removes the
		// private staging destination, its possible promotion, and the final range copy, so a broader destination cost
		// marker must not intercept it before the exact strategy is selected.
		return false;
	}
	else if constexpr (
		!((::fast_io::scatter_printable_for<ch_type, Args &> &&
		   !::fast_io::borrowed_scatter_source<ch_type, Args>) ||
		  ...) &&
		!::fast_io::details::decay::
			basic_general_concat_ordered_staging_complete_neutral_preferred_destination<
				ch_type, T>)
	{
		// Without an immediate-consumption barrier, only an explicit destination cost proof may replace the grouped plan.
		return false;
	}
	else
	{
		return concat_ordered_staging_minimum_leaf_count(
				   ::fast_io::io_strlike_type<ch_type, T>) <= sizeof...(Args);
	}
}();

/// @brief Runtime-only monotonic destination for ordered concat emission.
/// @details The state machine has exactly two externally observable states. Before promotion, three cached cursors
///          address the historical two-KiB physical safety window. At each compile-time pair boundary which precedes
///          another source,
///          an actual prefix over 512 characters constructs a fresh final result, copies and publishes that prefix, and
///          permanently redirects those cursors. A reserve beyond the physical window performs the same switch earlier
///          so no writer can overrun staging. Raw storage keeps the final object unconstructed on the measured short
///          path; the null/non-null result pointer is both its lifetime guard and the monotonic state. Cursor publication
///          into the final object is deferred until growth or return under the destination's explicit semantic proof.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
struct basic_concat_ordered_adaptive_buffer
{
	using char_type = ch_type;
	using result_type = T;
	inline static constexpr ::std::size_t buffer_size{
		::fast_io::details::basic_concat_buffer<char_type>::buffer_size};
	inline static constexpr ::std::size_t promotion_threshold{
		buffer_size < 512u ? buffer_size : 512u};
	inline static constexpr ::std::size_t promotion_capacity_floor{buffer_size * 2u};

	char_type stack_buffer[buffer_size];
	char_type *buffer_begin{stack_buffer};
	char_type *buffer_current{stack_buffer};
	char_type *buffer_end{stack_buffer + buffer_size};
	alignas(result_type) unsigned char result_storage[sizeof(result_type)];
	result_type *result_pointer{};

	inline basic_concat_ordered_adaptive_buffer() noexcept = default;
	basic_concat_ordered_adaptive_buffer(basic_concat_ordered_adaptive_buffer const &) = delete;
	basic_concat_ordered_adaptive_buffer &operator=(basic_concat_ordered_adaptive_buffer const &) = delete;

	inline ~basic_concat_ordered_adaptive_buffer() noexcept
	{
		if (result_pointer != nullptr)
		{
			::std::destroy_at(result_pointer);
		}
	}

	[[nodiscard]] inline result_type *construct_result() noexcept
	{
		// Store the returned pointer immediately. If the following reserve throws, stack unwinding still destroys the
		// now-live object; no source CPO has to be retried to reconstruct either its prefix or destination state.
		result_pointer = ::std::construct_at(reinterpret_cast<result_type *>(result_storage));
		return result_pointer;
	}
};

/// @brief Reserves the promoted result through the exact ordinary or runtime put-area family proved by its concept.
/// @details This helper deliberately does not subtract default cursors: a valid empty native string may represent all
///          three pointers as null before its first allocation, and null-pointer subtraction is not a capacity proof.
template <::std::integral ch_type, typename T>
	requires ::fast_io::output_buffer_strlike<ch_type, T>
inline void basic_concat_ordered_adaptive_target_reserve(T &result, ::std::size_t capacity)
{
	if constexpr (::fast_io::buffer_strlike<ch_type, T>)
	{
		strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, result, capacity);
	}
	else
	{
		strlike_runtime_reserve(::fast_io::io_strlike_type<ch_type, T>, result, capacity);
	}
}

/// @brief Promotes a full adaptive staging area exactly once and publishes its already completed prefix.
/// @details Promotion contains no source operation. The ordinary path enters between completed source pairs; an
///          oversized leaf instead enters from its explicit reserve boundary, where adaptive growth reacquires every
///          cursor before copying the still-unconsumed descriptor. Thus even a scratch-backed scatter is completely
///          consumed before a later source CPO can overwrite it. Reserving two physical staging windows folds the
///          destination's otherwise immediate first geometric growth into the promotion allocation. Pair batching
///          removes redundant hot checks; double writing is bounded by the 512-character threshold plus two complete
///          leaves and never exceeds the physical safety window.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline void basic_concat_ordered_adaptive_promote(
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer,
	::std::size_t requested_capacity)
{
	using buffer_type =
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>;
	T *const result{buffer.construct_result()};
	::std::size_t const capacity{
		requested_capacity < buffer_type::promotion_capacity_floor
			? buffer_type::promotion_capacity_floor
			: requested_capacity};
	::fast_io::details::decay::basic_concat_ordered_adaptive_target_reserve<ch_type>(
		*result, capacity);
	::std::size_t const prefix_size{
		static_cast<::std::size_t>(buffer.buffer_current - buffer.stack_buffer)};
	::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{result};
	ch_type *const target_begin{::fast_io::obuffer_begin(destination)};
	ch_type *current{::fast_io::freestanding::non_overlapped_copy_n(
		buffer.stack_buffer, prefix_size, target_begin)};
	// Cache the promoted put area without repeatedly decoding the final string's ABI in every later cursor CPO. The
	// destination's deferred-commit marker proves the copied prefix remains writable and owned until publication.
	buffer.buffer_begin = target_begin;
	buffer.buffer_current = current;
	buffer.buffer_end = ::fast_io::obuffer_end(destination);
	::fast_io::obuffer_set_curr(destination, current);
}

/// @brief Applies the actual-size transition at a source-pair boundary which has a following leaf.
/// @details The two source dispatchers have returned before this check, so no formatter retains a local cursor. The
///          destination may therefore relocate before the next source is invoked. Omitting the final-record check is
///          deliberate: range construction already performs the one necessary copy when no subsequent write can benefit
///          from promotion, while an actual physical overflow still promotes synchronously inside its current leaf.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
inline void basic_concat_ordered_adaptive_promote_between_pairs(
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer)
{
	using buffer_type =
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>;
	if (buffer.result_pointer == nullptr)
	{
		::std::size_t const actual_size{
			static_cast<::std::size_t>(buffer.buffer_current - buffer.stack_buffer)};
		// Equality belongs to the promoted side.  For the measured eight-leaf boundary, two complete balanced leaves
		// produce exactly 512 characters; delaying that state until the next checkpoint doubles the retained prefix and
		// the eventual promotion copy without protecting any additional short record.  Records whose complete payload is
		// near this threshold still reach their final pair without another checkpoint and retain range construction.
		if (buffer_type::promotion_threshold <= actual_size)
		{
			::fast_io::details::decay::basic_concat_ordered_adaptive_promote(
				buffer, actual_size);
		}
	}
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline ch_type *strlike_begin(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>,
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer) noexcept
{
	return buffer.buffer_begin;
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline ch_type *strlike_curr(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>,
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer) noexcept
{
	return buffer.buffer_current;
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline ch_type *strlike_end(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>,
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer) noexcept
{
	return buffer.buffer_end;
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
inline void strlike_set_curr(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>,
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer,
	ch_type *current) noexcept
{
	buffer.buffer_current = current;
}

/// @brief Preserves amortized growth after an ordered destination has left its private staging array.
/// @details A reserve CPO promises sufficient capacity, not geometric growth: both the native string and libc++ may
///          allocate only the requested extent. For a genuine miss, doubling the actual live capacity bounds repeated
///          prefix relocation geometrically without observing or replaying another source. The checked limit admits
///          pointer differences, byte allocation, and one terminal character. Near that limit, optional slack is
///          abandoned in favor of the caller's already validated request; it must not manufacture a maximum-sized
///          allocation or an overflow. A fitting reserve remains a no-op rather than acquiring speculative capacity.
template <::std::integral ch_type>
[[nodiscard]] inline constexpr ::std::size_t basic_concat_ordered_adaptive_growth_capacity(
	::std::size_t current_capacity, ::std::size_t requested) noexcept
{
	constexpr ::std::size_t contiguous_limit{
		::fast_io::details::decay::print_contiguous_char_extent_max_chars<ch_type>()};
	constexpr ::std::size_t terminated_byte_limit{SIZE_MAX / sizeof(ch_type) - 1u};
	constexpr ::std::size_t growth_limit{
		contiguous_limit < terminated_byte_limit ? contiguous_limit : terminated_byte_limit};
	if (current_capacity < requested && current_capacity <= growth_limit / 2u)
	{
		::std::size_t const doubled{current_capacity * 2u};
		if (requested < doubled)
		{
			return doubled;
		}
	}
	return requested;
}

template <bool amortize_growth, ::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
inline void basic_concat_ordered_adaptive_reserve(
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer,
	::std::size_t capacity)
{
	if (buffer.result_pointer == nullptr)
	{
		if (capacity <= buffer.buffer_size)
		{
			return;
		}
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote(buffer, capacity);
		return;
	}
	::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{buffer.result_pointer};
	// Only a promoted result reaches this path. Its cached endpoints designate one live allocation, so capacity is
	// available without inspecting a default string's possibly null cursors or performing any source customization.
	if constexpr (amortize_growth)
	{
		capacity = ::fast_io::details::decay::basic_concat_ordered_adaptive_growth_capacity<ch_type>(
			static_cast<::std::size_t>(buffer.buffer_end - buffer.buffer_begin), capacity);
	}
	// Publish the deferred logical end before a reserve may relocate the final allocation, then reacquire every cursor.
	// No source operation occurs here, so an allocation failure cannot expose a stale adapter or require source replay.
	::fast_io::obuffer_set_curr(destination, buffer.buffer_current);
	::fast_io::details::decay::basic_concat_ordered_adaptive_target_reserve<ch_type>(
		*buffer.result_pointer, capacity);
	buffer.buffer_begin = ::fast_io::obuffer_begin(destination);
	buffer.buffer_current = ::fast_io::obuffer_curr(destination);
	buffer.buffer_end = ::fast_io::obuffer_end(destination);
}

/// @brief Keeps ordinary reserve calls on the open-ended geometric-growth policy.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
inline void strlike_reserve(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>,
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer,
	::std::size_t capacity)
{
	::fast_io::details::decay::basic_concat_ordered_adaptive_reserve<true>(buffer, capacity);
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline constexpr ::std::size_t strlike_sso_size(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>) noexcept
{
	return ::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>::buffer_size;
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline constexpr ::std::true_type strlike_buffered_print_preferred(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>) noexcept
{
	return {};
}

template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
[[nodiscard]] inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	::fast_io::io_strlike_type_t<ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>) noexcept
{
	// The local cursor is a plain pointer publication; after the one-way switch, the final target's independent marker
	// supplies the same stability and publication proof. No deferred operation can observe both backing arrays.
	return {};
}

/// @brief Rejects an ordered candidate when its concrete output would select an intercepted print protocol.
/// @details Ordered construction deliberately invokes the ordinary dispatcher once per normalized leaf, always without
///          line ownership. A source namespace may define `status_print_define<false>` or `print_define` only for this
///          exact staging adapter even when the historical dummy-output `printable` probe is false. Invoking such a hook
///          could observe the adaptive result's deferred logical end, mutate its allocation behind the cached cursors,
///          or simply change concat's established semantics; merely making an exact direct CPO callable also invalidates
///          the earlier proof that the normalized source owns no destination-dependent representation. A mutex projection
///          would likewise introduce per-leaf locking absent from that proof. The actual adaptive or fixed staging type
///          is complete at this point, so these exact ADL checks close the open-world edge before optimization selection.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool basic_general_concat_ordered_staging_dispatch_safe() noexcept
{
	if constexpr (!::fast_io::details::decay::basic_general_concat_ordered_staging_candidate_v<
					  ch_type, T, Args...>)
	{
		return false;
	}
	else
	{
		auto const output_safe = []<typename output_type>() consteval {
			return !::fast_io::operations::decay::defines::
					has_output_or_io_stream_mutex_ref_define<output_type> &&
				   (!(::fast_io::operations::decay::defines::has_status_print_define<
						  false, output_type, Args>) &&
					...) &&
				   (!(::fast_io::details::direct_printable_to<
						  ch_type, output_type, Args>) &&
					...);
		};
		bool staging_safe{};
		if constexpr (
			::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
				ch_type, T>)
		{
			using staging_output = ::fast_io::io_strlike_reference_wrapper<
				ch_type,
				::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>;
			staging_safe = output_safe.template operator()<staging_output>();
		}
		else
		{
			using staging_type = ::fast_io::details::basic_concat_buffer<ch_type>;
			using staging_output = ::std::remove_reference_t<decltype(io_strlike_ref(
				::fast_io::io_alias, ::std::declval<staging_type &>()))>;
			staging_safe = output_safe.template operator()<staging_output>();
		}
		if (!staging_safe)
		{
			return false;
		}
		auto const whole_output_safe = []<typename output_type>() consteval {
			return !::fast_io::operations::decay::defines::
					has_output_or_io_stream_mutex_ref_define<output_type> &&
				   !::fast_io::operations::decay::defines::has_status_print_define<
					   line, output_type, Args...>;
		};
		if constexpr (
			::fast_io::details::decay::basic_general_concat_direct_destination_ok<
				line, ch_type, T, Args...>)
		{
			using original_output = ::std::remove_reference_t<decltype(io_strlike_ref(
				::fast_io::io_alias, ::std::declval<T &>()))>;
			// A whole-record status owner or destination lock belongs to the original adapter. Ordered private staging
			// cannot replace either operation with independent per-leaf output followed by range construction.
			return whole_output_safe.template operator()<original_output>();
		}
		else if constexpr (::fast_io::buffer_strlike<ch_type, T>)
		{
			using original_output = ::fast_io::io_strlike_reference_wrapper<ch_type, T>;
			return whole_output_safe.template operator()<original_output>();
		}
		else
		{
			using original_staging = ::fast_io::details::basic_concat_buffer<ch_type>;
			using original_output = ::std::remove_reference_t<decltype(io_strlike_ref(
				::fast_io::io_alias, ::std::declval<original_staging &>()))>;
			return whole_output_safe.template operator()<original_output>();
		}
	}
}

/// @brief Selects ordered staging only after cost, source equivalence, and exact-dispatch safety are proved.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_ordered_staging_run_v =
	::fast_io::details::decay::basic_general_concat_ordered_staging_dispatch_safe<
		line, ch_type, T, Args...>();

/// @brief Establishes one complete writable interval in the adaptive destination without re-reading adapter cursors.
/// @details The private state invariant is `B <= C <= E`, with all three pointers in one live array and `C` denoting
///          the unpublished logical end. A hit `N <= E - C` proves directly that the requested element and byte extents
///          fit the live same-array interval, so neither an independent global-extent check nor endpoint arithmetic is
///          required before the writer. Only a miss forms an absolute request: the checked `(C - B) + N` is passed to
///          the adaptive reserve CPO, which either performs the one-way stack-to-result transition or publishes the final
///          prefix before relocating its allocation. That CPO re-establishes `B <= C <= E` and `N <= E - C`; its
///          destination marker proves that allocating from an upper bound preserves the accepted exception boundary.
template <bool amortize_growth = true, ::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
		ch_type, T>
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_concat_ordered_adaptive_ensure(
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> &buffer,
	::std::size_t required)
{
	::std::size_t const available{
		static_cast<::std::size_t>(buffer.buffer_end - buffer.buffer_current)};
	if (required <= available) [[likely]]
	{
		return;
	}
	::std::size_t const used{
		static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin)};
	::std::size_t const requested{
		::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
			used, required)};
	if (requested == SIZE_MAX) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	::fast_io::details::decay::basic_concat_ordered_adaptive_reserve<amortize_growth>(buffer, requested);
}

/// @brief Emits one unambiguous reserve/scatter leaf directly through the adaptive buffer's cached cursor state.
/// @details This overload preserves the ordinary single-leaf priority proved by
///          `basic_general_concat_ordered_one_shot_leaf_v`: static reserve wins over dynamic reserve when both exist,
///          while scatter is admitted only when it is the sole contiguous representation. A size or descriptor CPO is
///          evaluated exactly once, any physical miss is resolved before its writer/copy, and `buffer_current` is then
///          advanced exactly once. No cursor CPO, generic output-policy branch, temporary payload, or intermediate
///          publication remains in the hot leaf graph. A scatter descriptor is copied before this function returns, so
///          neither a pair checkpoint nor a later producer can observe it. AddressSanitizer's explicit buffer-poisoning
///          mode retains the generic dynamic-reserve materialization path through the overload constraint below.
template <bool amortize_growth = true, ::std::integral ch_type, typename T, typename Arg>
	requires(
		::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
			ch_type, Arg> &&
		(::fast_io::details::asan_state::current !=
			 ::fast_io::details::asan_state::activate ||
		 ::fast_io::reserve_printable<ch_type, Arg> ||
		 !::fast_io::dynamic_reserve_printable<ch_type, Arg>))
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_general_concat_ordered_emit_one(
	::fast_io::io_strlike_reference_wrapper<
		ch_type,
		::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T>>
		&destination,
	Arg &arg)
{
	using value_type = ::std::remove_cvref_t<Arg>;
	auto &buffer{*destination.ptr};
	// Only the terminal leaf knows that no later source can amortize spare capacity. It uses an exact growth request
	// while earlier leaves retain geometric growth. This type-level fact neither predicts actual formatter length
	// from its upper bound nor evaluates a later CPO before its turn. Newline allocation remains after this writer.
	if constexpr (::fast_io::reserve_printable<ch_type, Arg>)
	{
		constexpr ::std::size_t required{
			print_reserve_size(::fast_io::io_reserve_type<ch_type, value_type>)};
		::fast_io::details::decay::basic_concat_ordered_adaptive_ensure<amortize_growth>(buffer, required);
		buffer.buffer_current = print_reserve_define(
			::fast_io::io_reserve_type<ch_type, value_type>,
			buffer.buffer_current, arg);
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<ch_type, Arg>)
	{
		::std::size_t const required{print_reserve_size(
			::fast_io::io_reserve_type<ch_type, value_type>, arg)};
		::fast_io::details::decay::basic_concat_ordered_adaptive_ensure<amortize_growth>(buffer, required);
		buffer.buffer_current = print_reserve_define(
			::fast_io::io_reserve_type<ch_type, value_type>,
			buffer.buffer_current, arg);
	}
	else
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<ch_type, value_type>, arg)};
		::fast_io::details::decay::basic_concat_ordered_adaptive_ensure<amortize_growth>(buffer, scatter.len);
		if (scatter.len != 0u)
		{
			buffer.buffer_current = ::fast_io::freestanding::non_overlapped_copy_n(
				scatter.base, scatter.len, buffer.buffer_current);
		}
	}
}

/// @brief Emits a normalized ordered concat record through one already selected destination adapter.
/// @details The quotient for a homogeneous pack changes only the generated call graph: each pointer still names the
///          original phase-one decay object, and one dispatch completes before the next pointer is loaded. A physical
///          overflow remains inside its current leaf; a threshold promotion occurs only after its complete pair. Neither
///          transition can defer an unretained descriptor across the invocation of a later source CPO.
template <bool amortize_growth = true, typename Destination, typename Arg>
inline constexpr void basic_general_concat_ordered_emit_one(
	Destination &destination, Arg &arg)
{
	::fast_io::operations::decay::print_freestanding_decay_impl<false>(destination, arg);
}

/// @brief Appends the final newline only after every source writer has completed.
/// @details A terminal writer whose own bound fits must run before a later newline allocation can fail. Reserving
///          both in advance would move that failure across an observable source CPO without a semantic proof. This
///          separate terminal step preserves the complete ordinary character dispatcher, including its overflow edge.
///          Even after the writer, pre-reserving one character would turn a miss into a hit and suppress an ADL
///          character-put or bulk-write overflow customization. The adaptive promotion/publication markers do not
///          authorize that observable substitution; terminal reserve sizing is therefore left to the selected CPO.
template <::std::integral ch_type, typename Destination>
inline constexpr void basic_general_concat_ordered_emit_line(Destination &destination)
{
	::fast_io::operations::decay::char_put_decay_dispatch(
		destination, ::fast_io::char_literal_v<u8'\n', ch_type>);
}

/// @brief Expands a heterogeneous ordered run two leaves at a time without a run-time checkpoint counter.
/// @details Each pair is fully emitted before the destination-only callback. A callback is instantiated only when a
///          following source exists, so the completed final pair retains the cheaper single range construction path.
template <typename Destination, typename AfterPair, typename Arg>
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_general_concat_ordered_emit_pairs(
	Destination &destination, AfterPair &, Arg &arg)
{
	::fast_io::details::decay::basic_general_concat_ordered_emit_one<false>(
		destination, arg);
}

template <typename Destination, typename AfterPair, typename First, typename Second,
		  typename... Rest>
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_general_concat_ordered_emit_pairs(
	Destination &destination, AfterPair &after_pair, First &first, Second &second,
	Rest &...rest)
{
	::fast_io::details::decay::basic_general_concat_ordered_emit_one(
		destination, first);
	::fast_io::details::decay::basic_general_concat_ordered_emit_one<(sizeof...(Rest) != 0u)>(destination, second);
	if constexpr (sizeof...(Rest) != 0u)
	{
		after_pair();
		::fast_io::details::decay::basic_general_concat_ordered_emit_pairs(
			destination, after_pair, rest...);
	}
}

template <bool line, ::std::integral ch_type, typename Destination, typename AfterPair,
		  typename... Args>
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
// Clang 23 otherwise outlines this one-use driver and its pair callback after the adaptive slow path raises the
// surrounding cost estimate. Keeping the driver in its sole caller removes those call frames; promotion itself stays
// cold and non-inlined, so this attribute does not replicate allocation or string-growth code.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void basic_general_concat_ordered_emit(
	Destination &destination, AfterPair &after_pair, Args &...args)
{
	// Type selection must not introduce a comma-expression requirement on a printable class. Taking built-in
	// addresses first makes every fold operand a pointer, excludes overloaded comma/address-of, and preserves the
	// exact cv-qualified final decay object's type without evaluating any source operation.
	using last_expression_type = ::std::remove_pointer_t<decltype((__builtin_addressof(args), ...))>;
	if constexpr (
		8u <= sizeof...(Args) &&
		(::std::same_as<last_expression_type, ::std::remove_reference_t<Args>> && ...))
	{
		last_expression_type *arguments[]{__builtin_addressof(args)...};
		::std::size_t index{};
		// Peel the terminal source at compile time. The open-ended loop has no per-iteration finality branch and each
		// completed pair still precedes its promotion checkpoint and the next source invocation.
		for (; index + 2u < sizeof...(Args); index += 2u)
		{
			::fast_io::details::decay::basic_general_concat_ordered_emit_one(
				destination, *arguments[index]);
			::fast_io::details::decay::basic_general_concat_ordered_emit_one(
				destination, *arguments[index + 1u]);
			after_pair();
		}
		if constexpr ((sizeof...(Args) & 1u) == 0u)
		{
			::fast_io::details::decay::basic_general_concat_ordered_emit_one(
				destination, *arguments[index]);
			++index;
		}
		::fast_io::details::decay::basic_general_concat_ordered_emit_one<false>(
			destination, *arguments[index]);
	}
	else
	{
		::fast_io::details::decay::basic_general_concat_ordered_emit_pairs(
			destination, after_pair, args...);
	}
	if constexpr (line)
	{
		::fast_io::details::decay::basic_general_concat_ordered_emit_line<ch_type>(destination);
	}
}

/// @brief Executes adaptive ordered emission and transfers the single live result when promotion occurred.
/// @details Short records retain range construction from the private array. Long records move the already published
///          final object; neither return arm invokes a source CPO, and destruction of the moved-from object closes the
///          manually started lifetime exactly once.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
			line, ch_type, T, Args...> &&
		::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
			ch_type, T>)
inline T basic_general_concat_ordered_adaptive_construct(Args &...args)
{
	::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<ch_type, T> buffer;
	{
		::fast_io::io_strlike_reference_wrapper<
			ch_type, decltype(buffer)> destination{__builtin_addressof(buffer)};
		auto after_pair{[&buffer] {
			::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(
				buffer);
		}};
		::fast_io::details::decay::basic_general_concat_ordered_emit<line, ch_type>(
			destination, after_pair, args...);
	}
	if (buffer.result_pointer != nullptr)
	{
		// Complete the one deferred publication before moving the result out of its manually managed lifetime.
		::fast_io::obuffer_set_curr(
			::fast_io::io_strlike_reference_wrapper<ch_type, T>{buffer.result_pointer},
			buffer.buffer_current);
		return static_cast<T &&>(*buffer.result_pointer);
	}
	return strlike_construct_define(
		::fast_io::io_strlike_type<ch_type, T>, buffer.stack_buffer, buffer.buffer_current);
}

/// @brief Executes the destination-audited ordered staging construction.
/// @details Each completed leaf advances the private logical cursor before the next begins. The adaptive one-shot
///          overload updates its audited cached cursor directly; every other leaf publishes the equivalent cursor through
///          the ordinary adapter. This is the formal immediate-consumption boundary for scratch-backed scatter producers.
///          The adapter scope ends before range construction, so a future stateful adapter cannot defer publication past
///          the read of the completed prefix; an exception destroys only the private buffer and cannot publish a partial
///          result. The trailing line feed is appended once after the fold, preserving concat's record semantics without
///          instantiating a whole-pack planner.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
		line, ch_type, T, Args...>
inline constexpr T basic_general_concat_ordered_staging_construct(Args &...args)
{
	if constexpr (
		::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
			ch_type, T>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			return ::fast_io::details::decay::basic_general_concat_ordered_adaptive_construct<
				line, ch_type, T>(args...);
		}
	}
	basic_concat_buffer<ch_type> buffer;
	{
		auto destination{io_strlike_ref(::fast_io::io_alias, buffer)};
		auto after_pair{[]() constexpr noexcept {}};
		::fast_io::details::decay::basic_general_concat_ordered_emit<line, ch_type>(
			destination, after_pair, args...);
	}
	return strlike_construct_define(
		::fast_io::io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
}

/// @brief Recognizes a construct-only destination which prefers coalescing a pure dynamic-reserve run.
/// @details This destination policy is deliberately independent from the dynamic-reserve concept. The source protocol
///          proves only that an object-dependent upper bound and writer exist; it says nothing about whether a concrete
///          append adapter should grow once per component or whether staging plus range construction is cheaper. Exact
///          `true_type` matching gives an audited string integration that missing cost premise without changing custom
///          traits, allocators, platforms, or unrelated concat shapes.
template <typename ch_type, typename T>
concept basic_general_concat_dynamic_coalescing_preferred_destination =
	::std::integral<ch_type> && !::fast_io::buffer_strlike<ch_type, T> &&
	::fast_io::range_constructible_strlike<ch_type, T> && requires {
		{
			concat_dynamic_reserve_coalescing_preferred(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Selects the narrow fresh-result dynamic coalescing cost policy.
/// @details The two-leaf minimum leaves a singleton on its destination's direct SSO/append path. Every admitted leaf is
///          exclusively dynamic reserve, so the compact planner invokes exactly the same CPO family as the ordinary
///          mixed planner; an earlier all-retained-scatter branch remains authoritative for dual-protocol packs.
template <typename ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_dynamic_coalescing_run_v =
	2u <= sizeof...(Args) && sizeof...(Args) <= 64u &&
	::fast_io::details::decay::basic_general_concat_dynamic_coalescing_preferred_destination<ch_type, T> &&
	(::fast_io::details::decay::concat_exclusive_dynamic_reserve_leaf_v<ch_type, Args> && ...);

/// @brief Recognizes an expensive leaf whose exact-size protocol would repeat substantial formatting work.
/// @details This is an explicit source cost policy, not an inference from `dynamic_reserve_printable`: arbitrary dynamic
///          producers may be cheap, stateful, or deliberately optimized for exact sizing. A width semantic node is the
///          one non-dynamic representation admitted here because it supplies its own exact marker/bound pair and the
///          semantic emitter writes it directly. Exact `true_type` matching keeps an unrelated same-named customization
///          from silently changing concat's allocation strategy.
template <typename ch_type, typename T>
concept basic_general_concat_single_pass_bounded_source =
	::std::integral<ch_type> &&
	(::fast_io::dynamic_reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	 ::fast_io::details::decay::print_semantic_width_v<
		 ::std::remove_cvref_t<T>>) &&
	::fast_io::single_pass_bounded_materialization_source<ch_type, T>;

/// @brief Recognizes a fresh construct-only result whose cost policy prefers one bounded staging pass.
/// @details A writable-buffer destination already owns a direct reserve strategy and is excluded. The destination CPO
///          additionally limits this optimization to string implementations whose construction and allocation model
///          has been audited; range construction alone is not a profitability proof.
template <typename ch_type, typename T>
concept basic_general_concat_single_pass_bounded_destination =
	::std::integral<ch_type> && !::fast_io::buffer_strlike<ch_type, T> &&
	::fast_io::range_constructible_strlike<ch_type, T> && requires {
		{
			concat_single_pass_bounded_destination_preferred(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Caps one-pass concat output staging independently of the library-wide maximum stack policy.
/// @details One KiB matches the semantic emitter's existing inline-frame ceiling and covers both compact records and
///          measured 600-digit precision without turning a source marker into an unbounded caller-frame request. Wider
///          character domains receive the corresponding whole-element count, while a stricter configured print budget
///          remains authoritative.
template <::std::integral ch_type>
inline consteval ::std::size_t basic_general_concat_single_pass_bounded_stack_size() noexcept
{
	constexpr ::std::size_t preferred_bytes{1024u};
	constexpr ::std::size_t preferred_size{preferred_bytes / sizeof(ch_type)};
	constexpr ::std::size_t configured_size{
		::fast_io::details::decay::print_stack_buffer_max_size<ch_type>()};
	return configured_size < preferred_size ? configured_size : preferred_size;
}

/// @brief Selects the bounded frame from the normalized run shape.
/// @details A single expensive leaf may use the full one-KiB ceiling, which covers measured large-precision scalar
///          formatting without duplicating its conversion. Multi-leaf records use 512 bytes: their common result is
///          compact, and this smaller frame remains inside GCC's profitable inline threshold.
template <::std::integral ch_type, typename... Args>
inline consteval ::std::size_t basic_general_concat_single_pass_bounded_run_stack_size() noexcept
{
	constexpr ::std::size_t maximum_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_stack_size<ch_type>()};
	if constexpr (sizeof...(Args) == 1u)
	{
		return maximum_size;
	}
	else
	{
		constexpr ::std::size_t preferred_size{512u / sizeof(ch_type)};
		return maximum_size < preferred_size ? maximum_size : preferred_size;
	}
}

/// @brief Recognizes an exact leaf which may be measured speculatively for fresh bounded construction.
/// @details The cached-size proof makes repeated observation of the unchanged object stable, constant-time, and
///          non-throwing. The fresh-concat marker supplies the independent cost preference, while eager safety permits a
///          failed bounded attempt to continue through the ordinary strategy without making the first observation
///          visible. Emission must use the paired precise writer; its exact size is not capacity for an ordinary reserve
///          writer, which may use any part of its larger conservative bound.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_exact_component_v =
	::fast_io::cached_precise_reserve_printable<ch_type, T> &&
	::fast_io::concat_fresh_precise_resize_printable_preferred<ch_type, T> &&
	::fast_io::eager_materialization_safe_printable<ch_type, T>;

/// @brief Recognizes a replayable scatter leaf whose descriptor is its complete print semantics.
/// @details Bounded preflight and emission observe the projection separately. Borrowed provenance proves identical
///          bytes and length for both observations; the remaining markers exclude destination-state dependence, hidden
///          direct-print semantics, normalization-owned storage, and observable failed preflight. Requiring the named
///          projection to be non-throwing also prevents the optional preflight from moving an exception ahead of the
///          fresh-result construction strategy it may ultimately decline.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_scatter_component_v =
	::fast_io::scatter_printable_for<ch_type, T &> &&
	::fast_io::borrowed_scatter_source<ch_type, T> &&
	::fast_io::scatter_output_state_independent<ch_type, T> &&
	::fast_io::scatter_direct_print_equivalent<ch_type, T> &&
	::fast_io::copy_stable_borrowed_print_source<ch_type, T> &&
	::fast_io::eager_materialization_safe_printable<ch_type, T> &&
	requires(T &value) {
		{
			print_scatter_define(
				::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<T>>,
				value)
		} noexcept -> ::std::same_as<::fast_io::basic_io_scatter_t<ch_type>>;
	};

/// @brief Preserves the component relation used by the original bounded-source strategy.
/// @details This spelling is intentionally separate from the extended relation below. It keeps existing bounded-source
///          dispatch stable, including raw descriptors and ordinary static-reserve companions, while preventing a new
///          exact/scatter proof from retroactively strengthening any of those historical components.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_bounded_historical_component_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T> ||
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::basic_io_scatter_t<ch_type>>;

/// @brief Proves that one component has a writer understood by the extended bounded constructor.
/// @details A bounded source publishes its non-consuming upper bound, an exact-preferred source publishes the paired
///          exact protocol above, and a fully proved scatter publishes a replayable descriptor. Every other admitted
///          component has a compile-time reserve extent. Unmarked dynamic, precise, and scatter producers remain on the
///          ordinary path: one marked neighbor can never grant permission to observe or replay their state.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_bounded_component_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T> ||
	::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, T> ||
	::fast_io::details::decay::basic_general_concat_single_pass_scatter_component_v<ch_type, T> ||
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::basic_io_scatter_t<ch_type>>;

/// @brief Requires every writer newly moved ahead of result allocation to be observationally eager-safe.
/// @details Exact/scatter admission changes staging only when this complete fold holds. In particular, a precise leaf's
///          purity cannot authorize an arbitrary static-reserve companion: if allocation later fails, pre-allocation
///          formatting must still be observationally equivalent to not having formatted any source.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_bounded_extended_component_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<ch_type, T> &&
	::fast_io::eager_materialization_safe_printable<ch_type, T>;

/// @brief Restricts the extended cost model to compiler families with measured code-generation support.
/// @details The gate is future-open within each verified frontend/platform family. Earlier compilers retain the exact
///          pre-extension dispatcher, so accepting the semantic CPOs does not by itself perturb their inliner budget or
///          binary layout. Apple Clang is versioned separately because its backend and C++ runtime differ from Linux.
inline consteval bool basic_general_concat_single_pass_bounded_extended_codegen_supported() noexcept
{
#if defined(__GNUC__) && !defined(__clang__)
	return __GNUC__ >= 16;
#elif defined(__clang__) && defined(__APPLE__)
	return __clang_major__ >= 23;
#elif defined(__clang__)
	return __clang_major__ >= 22;
#else
	return false;
#endif
}

/// @brief Identifies a static-reserve companion whose capacity may substantially exceed its emitted extent.
/// @details Exact, bounded, and proved-scatter components publish object-specific extents during preflight. The remaining
///          static reserve family can publish only a type-level capacity, so a long mixed run may need the full concat
///          frame even when its final bytes are compact.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_conservative_static_component_v =
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> &&
	!::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T> &&
	!::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, T> &&
	!::fast_io::details::decay::basic_general_concat_single_pass_scatter_component_v<ch_type, T>;

/// @brief Chooses the bounded frame after the extended component protocol is known.
/// @details The historical multi-leaf path keeps its 512-byte frame. A long extended run containing a conservative
///          static-reserve companion may otherwise complete every safe object query and reject a compact result solely
///          because type-level capacity exceeds 512 bytes. Reusing the existing one-KiB concat ceiling for that precise
///          shape avoids the redundant fallback without penalizing all-exact runs with a larger frame. Short runs retain
///          512 bytes, and an over-ceiling source still returns the rejection sentinel before any emission.
template <::std::integral ch_type, typename... Args>
inline consteval ::std::size_t basic_general_concat_single_pass_bounded_effective_stack_size() noexcept
{
	if constexpr (
		8u < sizeof...(Args) &&
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_codegen_supported() &&
		(::fast_io::details::decay::basic_general_concat_single_pass_conservative_static_component_v<ch_type, Args> || ...))
	{
		return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_stack_size<ch_type>();
	}
	else
	{
		return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<ch_type, Args...>();
	}
}

/// @brief Selects the narrow one-pass bounded construction strategy after concat normalization.
/// @details The 32-leaf ceiling bounds template expansion and covers the measured 21-leaf formatted mixed record. At
///          least one explicitly expensive leaf supplies the reason to replace exact sizing or incremental destination
///          growth, and every companion must pass the no-tentative-object-query proof above. Ordinary integer/string
///          runs therefore retain their existing strategy and code generation.
template <::std::integral ch_type, typename T, typename... Args>
inline consteval bool basic_general_concat_single_pass_bounded_run() noexcept
{
	if constexpr (
		sizeof...(Args) == 0u || 32u < sizeof...(Args) ||
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_effective_stack_size<
			ch_type, Args...>() == 0u ||
		!::fast_io::details::decay::basic_general_concat_single_pass_bounded_destination<ch_type, T>)
	{
		return false;
	}
	else
	{
		constexpr bool historical_run{
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, Args> || ...) &&
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_historical_component_v<ch_type, Args> && ...)};
		constexpr bool has_extended_component{
			((::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, Args> ||
			  ::fast_io::details::decay::basic_general_concat_single_pass_scatter_component_v<ch_type, Args>) ||
			 ...)};
		// Scatter purity is eligibility, not profitability: a homogeneous scatter run already has a retained-descriptor
		// strategy. Only a bounded or exact-preferred source may justify staging the complete extended run.
		constexpr bool has_extended_reason{
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, Args> || ...) ||
			(::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, Args> || ...)};
		constexpr bool extended_run{
			::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_codegen_supported() &&
			has_extended_component && has_extended_reason &&
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_component_v<ch_type, Args> && ...)};
		return historical_run || extended_run;
	}
}

template <::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_single_pass_bounded_run_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_run<
		ch_type, T, Args...>();

/// @brief Returns one component's cheap upper bound under the strategy's repeatability proof.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
		ch_type, T>
inline constexpr ::std::size_t basic_general_concat_single_pass_bounded_component_size(
	T &value, ::std::size_t maximum_size)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T>)
	{
		return ::fast_io::single_pass_bounded_materialization_size_invoke<ch_type>(
			value, maximum_size);
	}
	else if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, T>)
	{
		return print_reserve_precise_size(
			::fast_io::io_reserve_type<ch_type, value_type>, value);
	}
	else if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_scatter_component_v<ch_type, T>)
	{
		return print_scatter_define(
				   ::fast_io::io_reserve_type<ch_type, value_type>, value)
			.len;
	}
	else if constexpr (::fast_io::reserve_printable<ch_type, value_type>)
	{
		return print_reserve_size(::fast_io::io_reserve_type<ch_type, value_type>);
	}
	else
	{
		// The historical component relation admits a raw scatter descriptor directly. Its extent is intrinsic to the
		// descriptor and therefore needs neither an object-dependent CPO observation nor a conservative reserve bound.
		return value.len;
	}
}

/// @brief Adds one component to a still-viable bounded run.
/// @details `total` includes every preceding leaf (and the optional line feed), so a marked source receives exactly the
///          remaining budget. Once a predecessor rejects the plan, later candidate CPOs are not observed at all. This
///          keeps optional strategy probing narrower than fallback emission and makes the limit contract compositional.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
		ch_type, T>
inline constexpr void basic_general_concat_single_pass_bounded_add_component(
	::std::size_t &total, ::std::size_t maximum_size, T &value)
{
	if (total == SIZE_MAX || maximum_size < total)
	{
		total = SIZE_MAX;
		return;
	}
	auto const remaining{maximum_size - total};
	auto const component_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_size<ch_type>(
			value, remaining)};
	if (component_size == SIZE_MAX || remaining < component_size)
	{
		total = SIZE_MAX;
		return;
	}
	total += component_size;
}

/// @brief Computes the complete cheap bound without invoking an unmarked object-dependent CPO.
template <bool line, ::std::integral ch_type, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
			ch_type, Args> &&
		...)
inline constexpr ::std::size_t basic_general_concat_single_pass_bounded_total_size(Args &...args)
{
	constexpr ::std::size_t maximum_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_effective_stack_size<
			ch_type, Args...>()};
	::std::size_t total{line ? 1u : 0u};
	(::fast_io::details::decay::basic_general_concat_single_pass_bounded_add_component<ch_type>(
		 total, maximum_size, args),
	 ...);
	return total;
}

/// @brief Emits one component with the same protocol that supplied its bounded-preflight extent.
/// @details Exact and scatter components are intentionally handled before the generic bounded semantic emitter. The
///          exact branch repeats only a cached stable size query and validates a pointer-reporting writer against that
///          endpoint. The scatter branch repeats the borrowed projection and copies exactly its proved descriptor.
///          Remaining components were sized from a reserve bound and therefore use the ordinary bounded semantic leaf
///          path. This protocol pairing is the local invariant which prevents a precise extent from being mistaken for
///          an ordinary reserve writer's capacity.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
		ch_type, T>
inline constexpr ch_type *basic_general_concat_single_pass_bounded_emit_component(
	ch_type *iter, T &value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T>)
	{
		// Preserve the same bounded-source priority used by the preflight size selector above.
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<
			ch_type, true>(iter, value);
	}
	else if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, T>)
	{
		::std::size_t const exact_size{print_reserve_precise_size(
			::fast_io::io_reserve_type<ch_type, value_type>, value)};
		ch_type *const expected_end{exact_size == 0u ? iter : iter + exact_size};
		using define_result = decltype(print_reserve_precise_define(
			::fast_io::io_reserve_type<ch_type, value_type>, iter, exact_size,
			value));
		if constexpr (::std::same_as<define_result, ch_type *>)
		{
			ch_type *const actual_end{print_reserve_precise_define(
				::fast_io::io_reserve_type<ch_type, value_type>, iter,
				exact_size, value)};
			if (actual_end != expected_end) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
		else
		{
			print_reserve_precise_define(
				::fast_io::io_reserve_type<ch_type, value_type>, iter,
				exact_size, value);
		}
		return expected_end;
	}
	else if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_scatter_component_v<ch_type, T>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<ch_type, value_type>, value)};
		return scatter.len == 0u
				   ? iter
				   : ::fast_io::details::non_overlapped_copy_n(
						 scatter.base, scatter.len, iter);
	}
	else
	{
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<
			ch_type, true>(iter, value);
	}
}

/// @brief Provides one shared leaf body for long extended runs whose repeated inline spellings dominate text size.
/// @details This is a cost boundary only; it delegates to the same protocol-paired emitter and therefore preserves every
///          exact endpoint, scatter-lifetime, and bounded-writer invariant. It is selected only for compiler-gated runs
///          longer than eight leaves, where amortizing one call per leaf can be evaluated against the eliminated copies.
///          For the Linux Clang 22 32-leaf mixed record, outlining the repeated conservative-static and exact bodies
///          reduced text by 21,184 bytes and runtime by 34.46%; all seven paired samples won. GCC retains ordinary
///          inlining because its independently measured construct boundary has the opposite cost response.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<ch_type, T>
#if defined(__clang__) &&                                                                          \
	((defined(__APPLE__) && __clang_major__ >= 23) || (!defined(__APPLE__) && __clang_major__ >= 22)) && \
	__has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr ch_type *basic_general_concat_single_pass_bounded_emit_component_compact(
	ch_type *iter, T &value)
{
	return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_emit_component<ch_type>(
		iter, value);
}

/// @brief Selects a complete cached-exact run for direct construction in an audited runtime put area.
/// @details Every source separately proves a cached stable size, a non-throwing pointer-reporting writer, eager
///          observation safety, and independence from destination relocation. The destination separately proves that a
///          fresh default result owns an empty runtime put area and permits one final cursor publication. Requiring a
///          portable precise-resize fallback keeps constant evaluation valid without exposing runtime spare capacity.
///          The two-to-32 leaf range avoids perturbing the established singleton policy and bounds template expansion.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_fresh_runtime_exact_direct_run_v =
	1u < sizeof...(Args) && sizeof...(Args) <= 32u &&
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_codegen_supported() &&
	::fast_io::concat_fresh_runtime_exact_direct_strlike<ch_type, T> &&
	::fast_io::precise_resize_writable_strlike<ch_type, T> &&
	(::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, Args> && ...) &&
	(::fast_io::output_growth_independent_precise_reserve_printable<ch_type, Args> && ...) &&
	requires(T &result, ch_type *cursor) {
		{ strlike_runtime_curr(::fast_io::io_strlike_type<ch_type, T>, result) } noexcept -> ::std::same_as<ch_type *>;
		{ strlike_runtime_end(::fast_io::io_strlike_type<ch_type, T>, result) } noexcept -> ::std::same_as<ch_type *>;
		{ strlike_runtime_set_curr(::fast_io::io_strlike_type<ch_type, T>, result, cursor) } noexcept -> ::std::same_as<void>;
	};

/// @brief Emits the measured all-exact run and proves its aggregate endpoint before cursor publication.
/// @details The per-leaf helper repeats only a cached stable size observation, pairs it with the precise writer, and
///          checks that writer's endpoint. The final comparison then proves by induction that the complete fold consumed
///          exactly the allocation extent, including the optional line terminator.
template <bool line, ::std::integral ch_type, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, Args> && ...)
#if defined(__clang__) &&                                                                     \
	((defined(__APPLE__) && __clang_major__ >= 23) || (!defined(__APPLE__) && __clang_major__ >= 22))
// Apple Clang 23 already inlines the eight-leaf form naturally. At 32 leaves the marker removed 1,016 bytes of Mach-O
// __text with neutral paired runtime. Linux Clang 22 instead added 1,177 text bytes but improved the direct exact record
// by 5.42%; the separate platform gates preserve both measured cost decisions for their future compiler families.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr ch_type *basic_general_concat_fresh_runtime_exact_emit(
	ch_type *first, ::std::size_t total_size, Args &...args)
{
	ch_type *current{first};
	auto emit_one = [&current](auto &arg) constexpr {
		if constexpr (
			8u < sizeof...(Args) &&
			::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_codegen_supported())
		{
			// Reuse the protocol-identical shared leaf at the independently measured long-pack boundary. Apple Clang 23
			// reduced the pp/N32 Mach-O __text from 21,272 to 6,000 bytes; seven paired samples were runtime-neutral
			// (131.15 ns versus 130.88 ns), so the 15,272-byte reduction is not purchased with a material hot-path cost.
			current = ::fast_io::details::decay::
				basic_general_concat_single_pass_bounded_emit_component_compact<ch_type>(current, arg);
		}
		else
		{
			current = ::fast_io::details::decay::basic_general_concat_single_pass_bounded_emit_component<ch_type>(
				current, arg);
		}
	};
	(emit_one(args), ...);
	if constexpr (line)
	{
		*current++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	ch_type *const expected_end{total_size == 0u ? first : first + total_size};
	if (current != expected_end) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return current;
}

/// @brief Constructs one all-exact result with one allocation and one runtime cursor commit.
/// @details Sizing is left-to-right and checked before reserve. Runtime reserve occurs before every writer, so allocation
///          failure cannot expose a formatting side effect; every admitted writer is non-throwing thereafter. The
///          cached-size and output-growth proofs make the second per-leaf size observation identical after relocation.
///          Each pointer-returning writer is checked by the paired leaf emitter, and the final aggregate endpoint is
///          checked independently before publication. Constant evaluation instead establishes a live exact range with
///          the portable resize CPO and performs the same checked emission into that range.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_fresh_runtime_exact_direct_run_v<
		line, ch_type, T, Args...>
inline constexpr T basic_general_concat_fresh_runtime_exact_direct_run(Args &...args)
{
	T result;
	::std::size_t total_size{line ? 1u : 0u};
	auto measure_one = [&total_size](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		::std::size_t const component_size{print_reserve_precise_size(
			::fast_io::io_reserve_type<ch_type, arg_type>, arg)};
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
			total_size, component_size);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	};
	(measure_one(args), ...);

	FAST_IO_IF_NOT_CONSTEVAL
	{
		strlike_runtime_reserve(::fast_io::io_strlike_type<ch_type, T>, result, total_size);
		ch_type *const first{strlike_runtime_curr(::fast_io::io_strlike_type<ch_type, T>, result)};
		ch_type *const last{strlike_runtime_end(::fast_io::io_strlike_type<ch_type, T>, result)};
		::std::ptrdiff_t const available{
			first == nullptr || last == nullptr ? (first == last && total_size == 0u ? 0 : -1) : last - first};
		if (available < 0 || total_size > static_cast<::std::size_t>(available)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		ch_type *const actual_end{
			::fast_io::details::decay::basic_general_concat_fresh_runtime_exact_emit<line, ch_type>(
				first, total_size, args...)};
		strlike_runtime_set_curr(::fast_io::io_strlike_type<ch_type, T>, result, actual_end);
	}
	else
	{
		ch_type *const first{strlike_precise_resize_and_get_begin(
			::fast_io::io_strlike_type<ch_type, T>, result, total_size)};
		(void)::fast_io::details::decay::basic_general_concat_fresh_runtime_exact_emit<line, ch_type>(
			first, total_size, args...);
	}
	return result;
}

/// @brief Emits one already-bounded normalized run once, then constructs the final result from its actual prefix.
/// @details Formatting occurs outside any destination callback, so a throwing CPO unwinds normally and cannot cross a
///          `resize_and_overwrite` boundary. The caller has checked `bounded_size <= stack_size`; validating the returned
///          cursor before construction prevents an invalid endpoint from becoming an observable range. As with every
///          reserve protocol, the declared bound is the producer's proof that writes themselves stay inside the slice.
///
///          GCC 13 and 14 do not reliably fuse this stack producer with the final range constructor at `-O3`. Removing
///          the attribute regressed a run-time-precision fixed scalar by 4.32% and 5.61%, respectively, and a GCC 13
///          run-time-width scalar by 3.53%; GCC 13 paid 3,264 extra text bytes for the forced boundary, while GCC 14 was
///          568 bytes smaller with it. The same A/B matrix measured GCC 11 and 12 directly: their retained controls were
///          neutral, with no stable benefit or regression, so the pre-15 policy deliberately remains continuous rather
///          than inventing an untested family boundary. Clang 23 produced byte-identical historical scalar binaries;
///          its extended multi-leaf specialization is evaluated independently below. On Linux Clang 22, forcing that
///          boundary regressed the eight-leaf mixed record by 6.48% but improved the 32-leaf record by 4.46%. The same
///          32-leaf change regressed GCC 16 by 8.35%, so the future-open extension is Clang-only and requires more than
///          eight leaves. GCC 15 had no consistent
///          benefit. On GCC 16 the historical marker also regressed the fixed-scalar probe by 25.6% and added 64 text
///          bytes, so newer GNU frontends deliberately retain ordinary inlining policy.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
		ch_type, T, Args...>
FAST_IO_GNU_ALWAYS_INLINE
inline constexpr T basic_general_concat_single_pass_bounded_construct_implementation(
		::std::size_t bounded_size, Args &...args)
{
	constexpr ::std::size_t stack_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_effective_stack_size<
			ch_type, Args...>()};
	ch_type buffer[stack_size];
	ch_type *const first{buffer};
	ch_type *const last{first + bounded_size};
	ch_type *actual_end{first};
	auto emit_one = [&actual_end](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		if constexpr (
			8u < sizeof...(Args) &&
			::fast_io::details::decay::basic_general_concat_single_pass_bounded_extended_codegen_supported() &&
			(::fast_io::details::decay::basic_general_concat_single_pass_conservative_static_component_v<ch_type, arg_type> ||
			 ::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<ch_type, arg_type>))
		{
			actual_end = ::fast_io::details::decay::
				basic_general_concat_single_pass_bounded_emit_component_compact<ch_type>(
					actual_end, arg);
		}
		else
		{
			actual_end = ::fast_io::details::decay::
				basic_general_concat_single_pass_bounded_emit_component<ch_type>(actual_end, arg);
		}
	};
	(emit_one(args), ...);
	if constexpr (line)
	{
		*actual_end++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			first, last, actual_end)) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return strlike_construct_define(
		::fast_io::io_strlike_type<ch_type, T>, first, actual_end);
}

/// @brief Selects the measured outer inlining boundary without changing the construction body.
/// @details The implementation is always fused into one of the wrappers below, so this predicate controls exactly the
///          boundary measured against the phase-one dispatcher. GNU frontends before 15 retain their historical policy.
///          Verified Clang families force only long extended runs; compact runs retain the frontend's ordinary decision
///          because Linux Clang 22 consistently favored that choice at eight leaves.
template <typename... Args>
inline constexpr bool basic_general_concat_single_pass_bounded_force_outer_inline_v =
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 15
	true;
#elif defined(__clang__) && defined(__APPLE__) && __clang_major__ >= 23
	8u < sizeof...(Args);
#elif defined(__clang__) && !defined(__APPLE__) && __clang_major__ >= 22
	8u < sizeof...(Args);
#else
	false;
#endif

/// @brief Retains the compiler's ordinary phase-one inlining decision for this bounded construction.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
		ch_type, T, Args...>
inline constexpr T basic_general_concat_single_pass_bounded_construct(
	::std::size_t bounded_size, Args &...args)
{
	return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_construct_implementation<
		line, ch_type, T>(bounded_size, args...);
}

/// @brief Forces only the independently measured profitable outer boundary.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<ch_type, T, Args...> &&
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_force_outer_inline_v<Args...>)
FAST_IO_GNU_ALWAYS_INLINE
inline constexpr T basic_general_concat_single_pass_bounded_construct_forced(
	::std::size_t bounded_size, Args &...args)
{
	return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_construct_implementation<
		line, ch_type, T>(bounded_size, args...);
}

template <typename T>
inline constexpr bool basic_general_concat_top_level_condition_v =
	::fast_io::details::decay::print_semantic_node<T> &&
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<decltype(::fast_io::details::decay::print_semantic_node_ref(::std::declval<T>()))>>;

template <typename... Args>
inline constexpr bool basic_general_concat_has_top_level_condition_v =
	(false || ... || ::fast_io::details::decay::basic_general_concat_top_level_condition_v<Args>);

template <typename continuation, typename T>
struct basic_general_concat_condition_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	template <typename... TailArgs>
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<T>(*valueptr), ::std::forward<TailArgs>(tail_args)...);
	}
};

template <::std::integral ch_type, typename continuation>
inline constexpr decltype(auto) basic_general_concat_select_condition(continuation &&cont)
{
	return ::std::forward<continuation>(cont)();
}

template <::std::integral ch_type, typename continuation, typename T, typename... Args>
inline constexpr decltype(auto) basic_general_concat_select_condition(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<T>)
	{
		auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
		if (node_ref.pred)
		{
			using branch_type = decltype(node_ref.t1);
			if constexpr (::std::same_as<::std::remove_cvref_t<branch_type>, ::fast_io::io_null_t>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
			}
			else if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<branch_type>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), node_ref.t1, ::std::forward<Args>(args)...);
			}
			else
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation,
																								  branch_type>{
						__builtin_addressof(cont), __builtin_addressof(node_ref.t1)},
					::std::forward<Args>(args)...);
			}
		}
		else
		{
			using branch_type = decltype(node_ref.t2);
			if constexpr (::std::same_as<::std::remove_cvref_t<branch_type>, ::fast_io::io_null_t>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
			}
			else if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<branch_type>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), node_ref.t2, ::std::forward<Args>(args)...);
			}
			else
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation,
																								  branch_type>{
						__builtin_addressof(cont), __builtin_addressof(node_ref.t2)},
					::std::forward<Args>(args)...);
			}
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
	{
		return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_select_condition_ref_continuation
{
	T *strptr;

	template <typename... Args>
	inline constexpr void operator()(Args &&...args) const
	{
		::fast_io::details::decay::basic_general_concat_decay_ref_impl<line, ch_type>(
			*strptr, ::std::forward<Args>(args)...);
	}
};

template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_select_condition_phase1_continuation
{
	template <typename... Args>
	inline constexpr T operator()(Args &&...args) const
	{
		// The enclosing phase already owns the normalized graph. Selected condition members are named lvalues, so recurse
		// through the borrowing body instead of reopening a by-value normalization boundary.
		return ::fast_io::details::decay::basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(
			args...);
	}
};

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void
basic_general_concat_decay_ref_impl_semantic_precise(T &str, Args &...args)
{
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		constexpr ::std::size_t static_bound{
			::fast_io::operations::decay::print_semantic_static_bounded_total_size<line, ch_type, Args...>()};
		if constexpr (static_bound != SIZE_MAX)
		{
			if constexpr (static_bound <= local_cap)
			{
				auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
				auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type, true>(
					first, args...)};
				strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
				return;
			}
		}
	}
	::std::size_t const precise_size{
		::fast_io::operations::decay::print_semantic_precise_total_size<line, ch_type>(args...)};
	if (precise_size == SIZE_MAX) [[unlikely]]
	{
		// Stream output could split this exact run, but concat must return one contiguous object. Reject the
		// unrepresentable result before reserve or pointer arithmetic observes the sentinel as a real size.
		::fast_io::fast_terminate();
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < precise_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type>(first, args...)};
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief Emits an exactly measurable semantic run directly into a portable resized destination.
/// @details A construct-only destination previously routed this case through `basic_concat_buffer`, whose inline-first
///          representation puts a 2-KiB character array in the caller and then range-constructs the real result. The
///          semantic precise-size proof is stronger: it describes the selected condition branches, width padding,
///          packs, and optional newline exactly. Combining that proof with `precise_resize_writable_strlike` permits
///          one resize and one direct emission without granting the destination a fictitious spare-capacity put area.
///
///          This implementation is intentionally separate from semantic bounded emission. A bound may exceed the
///          produced prefix and would publish uninitialized padding as string size; precise emission instead verifies
///          that its returned cursor equals the promised logical end. Sizing is complete before resize. If emission
///          throws, the local result owns any partially written characters and is destroyed before it can escape.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::precise_resize_writable_strlike<ch_type, T>
inline constexpr T basic_general_concat_semantic_precise_resize(Args &...args)
{
	T result;
	::std::size_t const precise_size{
		::fast_io::operations::decay::print_semantic_precise_total_size<line, ch_type>(args...)};
	if (static_cast<::std::size_t>(PTRDIFF_MAX) < precise_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	ch_type *const first{strlike_precise_resize_and_get_begin(
		::fast_io::io_strlike_type<ch_type, T>, result, precise_size)};
	ch_type *const actual_end{
		::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type>(
			first, args...)};
	ch_type *expected_end{first};
	if (precise_size != 0u)
	{
		expected_end += precise_size;
	}
	if (actual_end != expected_end) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return result;
}

/// @brief Materializes a semantic concat run from its run-time upper bound.
/// @details The destination capacity is the sum produced by the same semantic graph used for emission.  Bounded
///          leaves may consume less than their reserve maximum; therefore only the returned cursor, never the bound,
///          becomes the observable string length.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_semantic_bounded(T &str, Args &...args)
{
	::std::size_t const bounded_size{
		::fast_io::operations::decay::print_semantic_bounded_total_size<line, ch_type>(args...)};
	if (bounded_size == SIZE_MAX) [[unlikely]]
	{
		// A conservative aggregate is only a capacity optimization. Preserve the valid semantic leaves through the
		// maintained cursor adapter, which emits them in order without requesting the impossible summed reserve bound.
		// `buffer_strlike` is the capability proof at every call site; an optional associated `io_strlike_ref` is neither
		// required nor sufficient and therefore cannot control whether this exceptional path is well formed.
		::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{__builtin_addressof(str)};
		::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		return;
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < bounded_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, bounded_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, bounded_size);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type, true>(
		first, args...)};
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl(T &str, Args &...args)
{
	if constexpr (basic_general_concat_has_top_level_condition_v<Args...>)
	{
		::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::fast_io::details::decay::basic_general_concat_select_condition_ref_continuation<line, ch_type, T>{
				__builtin_addressof(str)},
			args...);
	}
	else if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
	{
		basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
	}
	else if constexpr (basic_general_concat_semantic_bounded_ok<ch_type, Args...>)
	{
		// Semantic runs without a precise protocol still have an object-specific capacity proof.  Keeping them in the
		// semantic emitter preserves width/condition/pack meaning across every string adapter.
		basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(str, args...);
	}
	else if constexpr (
		::fast_io::details::decay::concat_retained_reserve_scatters_run_v<ch_type, Args...>)
	{
		// Unlike ordinary stream output, concat owns the final contiguous destination. Retaining the complete static
		// plan lets it replace per-descriptor destination growth with one checked reserve and one ordered copy.
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters<line, ch_type>(
			str, args...);
	}
	else if constexpr ((concat_retained_scatter_printable_v<ch_type, Args> && ...))
	{
		// Exact retained descriptors outrank every reserve representation, including compiler-local mixed shortcuts.
		// A dual-protocol leaf may intentionally expose different reserve and scatter spellings; selecting the scatter
		// path here preserves the established all-retained protocol priority before either shortcut classifies leaves.
		basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(str, args...);
	}
	else if constexpr (16u <= sizeof...(Args) && sizeof...(Args) <= 64u &&
					   (::fast_io::details::decay::concat_exclusive_dynamic_reserve_leaf_v<
							ch_type, Args> &&
						...))
	{
		// Long pure dynamic-reserve runs need only one cached bound per normalized argument. Short runs keep the general
		// planner because their metadata savings do not repay a second code path. The preceding branch preserves
		// all-scatter priority.
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_dynamic<line, ch_type>(
			str, args...);
	}
#if defined(__GNUC__) && !defined(__clang__)
	else if constexpr (::fast_io::details::decay::concat_gcc_hot_mixed_run_v<ch_type, Args...>)
	{
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed_gcc_hot<line, ch_type>(
			str, args...);
	}
#endif
#if defined(__clang__)
	else if constexpr (::fast_io::details::decay::concat_small_mixed_run_v<ch_type, Args...>)
	{
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_small_mixed<line, ch_type>(
			str, args...);
	}
#endif
	else if constexpr (((reserve_printable<ch_type, Args> ||
						 concat_retained_scatter_printable_v<ch_type, Args> ||
						 dynamic_reserve_printable<ch_type, Args>) &&
						...))
	{
		if constexpr ((concat_retained_scatter_printable_v<ch_type, Args> && ...))
		{
			// Exact retained descriptors outrank reserve upper bounds. A dual-protocol leaf may advertise an enormous
			// conservative reserve while its borrowed scatter is tiny; rejecting the exact representation because the
			// unused bound aggregate is unavailable would be a protocol-priority inversion.
			basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(str, args...);
		}
		else
		{
			constexpr ::std::size_t reserve_size{
				calculate_concat_scatter_reserve_size_or_unavailable<ch_type, Args...>()};
			constexpr ::std::size_t reserve_size_with_line{
				::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
			if constexpr (reserve_size_with_line == SIZE_MAX)
			{
				// The static sum is only a conservative grouped-allocation proof. Reuse a per-leaf cached plan so an
				// unavailable aggregate never re-queries a stateful size customization or allocates the impossible sum.
				::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
					str, args...);
			}
			else if constexpr ((reserve_printable<ch_type, Args> && ...))
			{
				if constexpr (sso_buffer_strlike<ch_type, T>)
				{
					constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
					constexpr bool not_enough_space{(local_cap < reserve_size_with_line)};
					if constexpr (not_enough_space &&
								  ((sizeof...(Args) == 1) && (precise_reserve_printable<ch_type, Args> && ...)))
					{
						basic_general_concat_decay_ref_impl_precise<line, ch_type, T>(str, args...);
					}
					else
					{
						if constexpr (not_enough_space)
						{
							strlike_reserve(io_strlike_type<ch_type, T>, str, reserve_size_with_line);
						}
						strlike_set_curr(io_strlike_type<ch_type, T>, str,
										 print_reserve_define_chain_impl<line>(
											 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
					}
				}
				else
				{
					strlike_reserve(io_strlike_type<ch_type, T>, str, reserve_size_with_line);
					strlike_set_curr(
						io_strlike_type<ch_type, T>, str,
						print_reserve_define_chain_impl<line>(
							strlike_begin(io_strlike_type<ch_type, T>, str), args...));
				}
			}
			else
			{
				// Dynamic bounds and retained descriptors are object-dependent. Measure each selected protocol once,
				// cache its metadata, and share it between the grouped and sequential strategies.
				::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
					str, args...);
			}
		}
	}
	else
	{
		if constexpr (
			::fast_io::details::decay::basic_general_concat_direct_destination_ok<
				line, ch_type, T, Args...>)
		{
			decltype(auto) destination = io_strlike_ref(::fast_io::io_alias, str);
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		}
		else
		{
			static_assert(
				::fast_io::details::decay::basic_general_concat_generic_buffer_destination_ok<
					line, ch_type, T, Args...>,
				"the normalized concat fallback is not printable to the result's generic cursor adapter");
			::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{
				__builtin_addressof(str)};
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		}
	}
}

/// @brief Selects the phase-one materialization strategy for a normalized concat leaf run.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_ref_impl(Args &...args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			if constexpr (single_character_constructible_strlike<ch_type, T>)
			{
				return strlike_construct_single_character_define(io_strlike_type<ch_type, T>,
																 char_literal_v<u8'\n', ch_type>);
			}
			else if constexpr (::fast_io::range_constructible_strlike<ch_type, T>)
			{
				return strlike_construct_define(io_strlike_type<ch_type, T>,
												__builtin_addressof(char_literal_v<u8'\n', ch_type>),
												__builtin_addressof(char_literal_v<u8'\n', ch_type>) + 1);
			}
			else
			{
				// A writable-buffer-only result has no range constructor. Reuse the ordinary line emitter so the
				// generic cursor adapter owns reserve/growth and the one committed newline exactly as it does for a
				// nonempty run.
				T str;
				::fast_io::details::decay::basic_general_concat_decay_ref_impl<true, ch_type>(str);
				return str;
			}
		}
		else
		{
			return {};
		}
	}
	else
	{
		if constexpr (
			::fast_io::details::decay::basic_general_concat_fresh_runtime_exact_direct_run_v<
				line, ch_type, T, Args...>)
		{
			return ::fast_io::details::decay::basic_general_concat_fresh_runtime_exact_direct_run<
				line, ch_type, T>(args...);
		}
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
				ch_type, T, Args...>)
		{
			constexpr ::std::size_t stack_size{
				::fast_io::details::decay::basic_general_concat_single_pass_bounded_effective_stack_size<
					ch_type, Args...>()};
			::std::size_t const bounded_size{
				::fast_io::details::decay::basic_general_concat_single_pass_bounded_total_size<
					line, ch_type>(args...)};
			if (bounded_size <= stack_size) [[likely]]
			{
				if constexpr (
					::fast_io::details::decay::basic_general_concat_single_pass_bounded_force_outer_inline_v<
						Args...>)
				{
					return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_construct_forced<
						line, ch_type, T>(bounded_size, args...);
				}
				else
				{
					return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_construct<
						line, ch_type, T>(bounded_size, args...);
				}
			}
		}
		if constexpr (basic_general_concat_has_top_level_condition_v<Args...>)
		{
			return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
				::fast_io::details::decay::basic_general_concat_select_condition_phase1_continuation<line, ch_type,
																									 T>{},
				args...);
		}
		// A source-proved one-pass run must reach the result adapter before any replay-based sizing strategy is considered.
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_one_pass_direct_destination_run_v<
				line, ch_type, T, Args...>)
		{
			// The source cost proof intentionally wins before any precise/dynamic size query.  The fresh result owns all
			// append growth, and destruction contains a throwing formatter's unpublished partial value.  The adapter's
			// shorter lifetime also completes a possible destructor-based cursor publication before result movement.
			T str;
			{
				decltype(auto) destination{
					io_strlike_ref(::fast_io::io_alias, str)};
				::fast_io::details::decay::basic_general_concat_one_pass_direct_emit<line, ch_type>(
					destination, args...);
			}
			return str;
		}
		else if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
		{
			if constexpr (buffer_strlike<ch_type, T>)
			{
				T str;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
				return str;
			}
			else if constexpr (precise_resize_writable_strlike<ch_type, T>)
			{
				return ::fast_io::details::decay::basic_general_concat_semantic_precise_resize<
					line, ch_type, T>(args...);
			}
			else
			{
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(buffer, args...);
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
		else if constexpr (basic_general_concat_semantic_bounded_ok<ch_type, Args...>)
		{
			if constexpr (buffer_strlike<ch_type, T>)
			{
				T str;
				basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(str, args...);
				return str;
			}
			else
			{
				// Construct-only destinations receive only the prefix reported by bounded semantic emission; unused reserve
				// capacity never leaks into their value.
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(buffer, args...);
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
		// A marked singleton exact source owns this fresh-construction policy and must precede generic staging.
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_fresh_precise_resize_preferred_run_v<
				ch_type, T, Args...>)
		{
			// This marker belongs only to fresh-result construction.  Its exact source query is paired here with the
			// concrete result's reserve/commit or precise-resize capability before the generic buffer path can select an
			// upper-bound or incremental strategy.  Existing-string concat and ordinary stream print never reach this arm.
			return ::fast_io::details::decay::basic_general_concat_fresh_precise_resize_preferred_run<
				line, ch_type, T>(args...);
		}
		else if constexpr (
			basic_general_concat_context_staging_preferred_destination<ch_type, T> &&
			(::fast_io::context_printable<ch_type, Args> && ...))
		{
			// Context output is intrinsically single-pass: no rewind or purity contract permits an exact-size replay.
			// Advance each state exactly once into concat's inline-first buffer, then publish only the completed prefix by
			// range construction. If a producer throws, the staging buffer is destroyed and no partial result escapes.
			basic_concat_buffer<ch_type> buffer;
			auto destination{io_strlike_ref(::fast_io::io_alias, buffer)};
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
			return strlike_construct_define(
				io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_dynamic_coalescing_run_v<
				ch_type, T, Args...>)
		{
			FAST_IO_IF_NOT_CONSTEVAL
			{
				if constexpr (
					::fast_io::details::decay::basic_general_concat_runtime_dynamic_direct_destination<
						ch_type, T>)
				{
					// The audited run-time put area removes both staging storage and the final range copy while retaining
					// one reserve and one deferred logical-size publication.
					return ::fast_io::details::decay::basic_general_concat_fresh_runtime_dynamic_direct<
						line, ch_type, T>(args...);
				}
			}
			// The destination explicitly prefers one contiguous dynamic run over per-leaf append growth. The compact
			// planner is also the constant-evaluation fallback because a standard string cannot expose writable spare
			// capacity there. Range construction sees only returned prefixes and never unused conservative capacity.
			basic_concat_buffer<ch_type> buffer;
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_dynamic<line, ch_type>(
				buffer, args...);
			return strlike_construct_define(
				io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
		else if constexpr (buffer_strlike<ch_type, T>)
		{
			if constexpr (
				::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
					line, ch_type, T, Args...>)
			{
				// The native-string policy still requires an unretained scatter barrier. The staging dispatcher consumes
				// that descriptor immediately while its inline area avoids growing an empty result.
				return ::fast_io::details::decay::basic_general_concat_ordered_staging_construct<
					line, ch_type, T>(args...);
			}
			else
			{
				T str;
				basic_general_concat_decay_ref_impl<line, ch_type>(str, args...);
				return str;
			}
		}
		else if constexpr (
			line && sizeof...(Args) == 1u &&
			(::fast_io::details::decay::direct_scatter_view_printable<ch_type, Args> && ...) &&
			requires(::fast_io::basic_io_scatter_t<ch_type> scatter) {
				{
					strlike_construct_scatter_with_line_feed_define(
						::fast_io::io_strlike_type<ch_type, T>, scatter,
						::std::size_t{})
				} -> ::std::same_as<T>;
			})
		{
			// The result is freshly constructed, so its allocation cannot own the borrowed source
			// descriptor. Reserve the complete text-plus-LF extent before either append and avoid the
			// exact-capacity first append followed by a second allocation for the line feed.
			::fast_io::basic_io_scatter_t<ch_type> const scatter{args...};
			::std::size_t const total_size{
				::fast_io::details::decay::concat_precise_size_with_line<true>(scatter.len)};
			return strlike_construct_scatter_with_line_feed_define(
				::fast_io::io_strlike_type<ch_type, T>, scatter, total_size);
		}
		else if constexpr (
			sizeof...(Args) == 1u &&
			(::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
				 line, ch_type, T, Args> &&
			 ...))
		{
			// A newline-bearing run-time scatter range would otherwise be measured, copied into concat staging, and copied
			// again into the final standard string. The exact callback path owns the same two-pass source traversal but
			// writes the second pass and newline directly into the destination's uninitialized live extent. It precedes
			// retained-scatter and reserve fallbacks only for the independently proved one-leaf/noexcept line shape above.
			return ::fast_io::details::decay::basic_general_concat_exact_overwrite_run<
				line, ch_type, T>(args...);
		}
		else if constexpr ((!line) && sizeof...(args) == 1 &&
						   (::fast_io::scatter_printable_for<ch_type, Args &> && ...))
		{
			basic_io_scatter_t<ch_type> scatter{print_scatter_define_extract_one<ch_type>(args...)};
			if (::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(0u, scatter.len) == SIZE_MAX)
				[[unlikely]]
			{
				// Validate the exact descriptor before forming `base + len`; pointer arithmetic cannot itself be used as
				// the proof because an excessive length would already make that expression undefined.
				::fast_io::fast_terminate();
			}
			ch_type const *const first{
				scatter.len == 0u ? __builtin_addressof(::fast_io::char_literal_v<u8'\0', ch_type>) : scatter.base};
			return strlike_construct_define(io_strlike_type<ch_type, T>, first, first + scatter.len);
		}
		else if constexpr ((concat_retained_scatter_printable_v<ch_type, Args> && ...))
		{
			// Construct-only destinations cannot expose final writable storage. Materialize the exact retained scatter
			// run once in concat's growable staging string, including the optional newline, then construct from its used
			// prefix. This branch deliberately precedes reserve bounds for dual-protocol leaves.
			basic_concat_buffer<ch_type> buffer;
			basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(buffer, args...);
			return strlike_construct_define(
				io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
		else if constexpr ((reserve_printable<ch_type, Args> && ...))
		{
			FAST_IO_IF_NOT_CONSTEVAL
			{
				if constexpr (
					::fast_io::details::decay::
						basic_general_concat_fresh_fixed_scalar_direct_run_v<
							line, ch_type, T, Args...>)
				{
					// The result is fresh, and the complete dispatcher retains the adapter's line, status,
					// locking, growth, and cursor-publication semantics. Constant evaluation deliberately
					// continues through the portable staging path below because a standard string cannot
					// expose writable spare capacity there.
					T str;
					{
						// A third-party associated adapter may publish its final cursor or size from its destructor.
						// End that lifetime before `str` is moved into the return object; otherwise return-value
						// construction could observe the destination before its deferred commit becomes visible.
						decltype(auto) destination{
							io_strlike_ref(::fast_io::io_alias, str)};
						::fast_io::operations::decay::print_freestanding_decay_impl<
							line>(destination, args...);
					}
					return str;
				}
			}
			constexpr ::std::size_t reserve_size{
				calculate_concat_scatter_reserve_size_or_unavailable<ch_type, Args...>()};
			constexpr ::std::size_t reserve_size_with_line{
				::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
			if constexpr (reserve_size_with_line == SIZE_MAX)
			{
				if constexpr (
					::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
						ch_type, T, Args...>)
				{
					// A conservative reserve sum may be unavailable while the same leaves have a tiny exact protocol.
					// Prefer the destination's exact resize proof before allocating any per-leaf reserve scratch.
					return ::fast_io::details::decay::basic_general_concat_precise_resize_run<
						line, ch_type, T>(args...);
				}
				else
				{
					// Construct-only results cannot expose append storage. Cache the selected reserve metadata once in a
					// growable staging string and range-construct only from the prefixes actually returned by producers.
					basic_concat_buffer<ch_type> buffer;
					::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
						buffer, args...);
					return strlike_construct_define(
						io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
				}
			}
			else if constexpr (
				::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_size_with_line, ch_type>)
			{
				// Non-buffer string-like destinations cannot expose writable storage, so a small all-reserve run is
				// staged once before construction.  The shared byte budget is the proof that this automatic array cannot
				// make concat's frame larger than the print/scan policy permits.
				ch_type buffer[reserve_size_with_line];
				auto p{print_reserve_define_chain_impl<line>(buffer, args...)};
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer, p);
			}
			else
			{
				// A compile-time reserve bound is not a stack-safety guarantee.  Keeping the large alternative in dynamic
				// storage prevents a custom fixed-capacity printable from injecting an arbitrarily large frame into an
				// otherwise allocation-free concat instantiation; construction still observes the same exact [begin,end)
				// sequence and therefore has identical output semantics.
				::fast_io::details::local_operator_new_array_ptr<ch_type> buffer(reserve_size_with_line);
				auto p{print_reserve_define_chain_impl<line>(buffer.ptr, args...)};
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.ptr, p);
			}
		}
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
				line, ch_type, T, Args...>)
		{
			// Earlier exact scatter and static-reserve constructions retain priority. This arm covers the remaining
			// destination-neutral pack with one ordered inline-first emission and one final range construction.
			return ::fast_io::details::decay::basic_general_concat_ordered_staging_construct<
				line, ch_type, T>(args...);
		}
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
				ch_type, T, Args...>)
		{
			// Direct scatter construction and all-static reserve construction above retain priority. This later branch
			// targets a bounded ordinary run whose dynamic exact protocol can replace concat-buffer staging plus final
			// range construction with one logical resize and ordered in-place emission. Initialization-sensitive leaves
			// reach this branch only when the destination separately proves that establishing the live range does not
			// value-initialize characters which the formatter immediately overwrites.
			return ::fast_io::details::decay::basic_general_concat_precise_resize_run<line, ch_type, T>(args...);
		}
		else
		{
			if constexpr (
				!::fast_io::details::decay::basic_general_concat_initialization_sensitive_staging_required_v<
					ch_type, T, Args...> &&
				::fast_io::details::decay::basic_general_concat_direct_destination_ok<
					line, ch_type, T, Args...>)
			{
				// Append-oriented non-buffer results (notably std::basic_string on implementations where writable
				// internals are unavailable) can still receive the normalized run directly. This removes the intermediate
				// `basic_concat_buffer` allocation/materialization and the final range construction for direct/context
				// leaves; the destination may still allocate while it grows. Enter the ordinary print dispatcher on the
				// adapter itself: `basic_general_concat_decay_ref_impl` assumes that its `T` exposes writable strlike cursors
				// on reserve/scatter paths, which is exactly what this non-buffer branch does not promise.
				T str;
				{
					// Preserve a reference-returning adapter exactly and complete any RAII commit before the result is moved.
					decltype(auto) destination{
						io_strlike_ref(::fast_io::io_alias, str)};
					::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
				}
				return str;
			}
			else
			{
				static_assert(
					::fast_io::details::decay::basic_general_concat_staging_destination_ok<
						line, ch_type, Args...>,
					"the normalized concat fallback is printable to neither the result adapter nor its staging stream");
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl<line, ch_type>(buffer, args...);
				return strlike_construct_define(
					io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
	}
}

/// @brief Proves that direct compiler-constant result construction cannot bypass a whole-run status operation.
/// @details Both concat constant gates eventually write replacement proxies without entering a print dispatcher.  This
///          destination proof is therefore shared by the public-source gate and the later normalized phase-1 gate.  It
///          checks the complete source and replacement packs on every physical output concat can select, recursively
///          unwrapping a complete mutex protocol.  Merely protecting the public gate is insufficient: its ordinary
///          continuation reaches the normalized gate, which must independently preserve the same status semantics.
template <bool line, ::std::integral ch_type, typename T,
		  typename source_types, typename replacement_types>
inline consteval bool
basic_general_concat_compiler_constant_status_safe_for_types() noexcept
{
	auto const output_safe = []<typename output_type>() consteval {
		return ::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_output_safe<
				true, line, ::std::remove_cvref_t<output_type>,
				source_types, replacement_types>::value;
	};

	// Check an associated result adapter only when the destination actually supplies one.
	if constexpr (requires(T &result) {
					  io_strlike_ref(::fast_io::io_alias, result);
				  })
	{
		using associated_output = ::std::remove_reference_t<decltype(io_strlike_ref(::fast_io::io_alias, ::std::declval<T &>()))>;
		// A named character domain is needed before comparing the adapter with the requested concat domain.
		if constexpr (requires { typename associated_output::output_char_type; })
		{
			// Reject only a same-domain adapter whose whole-run status protocol would be bypassed by direct construction.
			if constexpr (
				::std::same_as<ch_type,
							   typename associated_output::output_char_type> &&
				!output_safe.template operator()<associated_output>())
			{
				return false;
			}
		}
	}
	// Native writable-string storage and staging storage have distinct output types and require separate status proofs.
	if constexpr (::fast_io::buffer_strlike<ch_type, T>)
	{
		using generic_output =
			::fast_io::io_strlike_reference_wrapper<ch_type, T>;
		// A status-owning generic adapter must retain the ordinary print dispatcher instead of direct proxy emission.
		if constexpr (!output_safe.template operator()<generic_output>())
		{
			return false;
		}
	}
	else
	{
		using staging_type = ::fast_io::details::basic_concat_buffer<ch_type>;
		using staging_output = ::std::remove_reference_t<decltype(io_strlike_ref(::fast_io::io_alias,
																				 ::std::declval<staging_type &>()))>;
		// Construct-only destinations are safe only when their concrete staging stream has no intercepted status run.
		if constexpr (!output_safe.template operator()<staging_output>())
		{
			return false;
		}
	}
	return true;
}

/// @brief Proves that this compiler can erase concat's complete replacement graph after a successful query.
/// @details A successful builtin query is useful only when this compiler can erase the complete replacement graph.
///          GCC 11 cannot do so for an exact-preferred replacement across concat result construction: recursive
///          assembly retains the precision planner/native exact formatter and a 1,280-byte frame. GCC 12--17 erase the
///          same graph. One flat integer proxy is the sole proved exception: its precise writer delegates directly to
///          one bounded integer leaf and has no planner graph. Paired GCC 11 percent-format roots erase the native
///          writer only for the literal while the unknown source retains that exact writer. Multi-leaf and mixed packs
///          remain rejected because the proof does not compose beyond that measured root. Rejecting every other
///          exact-preferred replacement keeps its query unexecuted and routes the source through concat's historical
///          run-time formatter; later compilers retain their independently proved materialization paths.
template <::std::integral ch_type, typename... ReplacementArgs>
inline consteval bool
basic_general_concat_compiler_constant_replacement_codegen_supported() noexcept
{
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 11
	if constexpr (
		sizeof...(ReplacementArgs) == 1u &&
		(::fast_io::compiler_constant_flat_integer_replacement<
			 ch_type, ::std::remove_cvref_t<ReplacementArgs>> &&
		 ...))
	{
		return true;
	}
	else
	{
		return !(false || ... ||
				 ::fast_io::compiler_constant_precise_compact_preferred<
					 ch_type, ::std::remove_cvref_t<ReplacementArgs>>);
	}
#else
	return true;
#endif
}

/// @brief Closes compiler-constant concat for destination/source pairs whose successful graph is not erasable.
/// @details GCC 11 retains the dynamic-precision replacement for every audited concat destination. GCC 12--17 erase
///          the same source for fast_io's writable string, but a construct-only default standard string retains proxy
///          and native formatter nodes after a successful query. That destination already owns a measured bounded
///          one-pass formatter, so this type-only proof keeps the query unexecuted and selects that ordinary strategy.
template <::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_compiler_constant_source_codegen_supported() noexcept
{
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
	if constexpr (
		(false || ... ||
		 ::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
			 ch_type, Args>))
	{
		return __GNUC__ != 11 &&
			   !::fast_io::details::decay::
				   basic_general_concat_single_pass_bounded_destination<ch_type, T>;
	}
#endif
	return true;
}

/// @brief Tests whether concat's active normalized leaf run has a useful compiler-constant replacement.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool basic_general_concat_compiler_constant_materialization_available() noexcept
{
	if constexpr (!::fast_io::details::decay::
					  basic_general_concat_compiler_constant_source_codegen_supported<
						  ch_type, T, Args...>())
	{
		return false;
	}
	else if constexpr (
		!(::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			  ch_type, Args> &&
		  ...))
	{
		// A semantically complete but unclassified provider has no consumer deletion proof. Keep concat's value query,
		// replacement type, and proxy writer structurally absent until the provider supplies the permanent graph proof.
		return false;
	}
	else if constexpr (!(::fast_io::compiler_constant_printable<ch_type, Args> && ...))
	{
		return false;
	}
	else if constexpr (!::fast_io::details::decay::
						   basic_general_concat_compiler_constant_replacement_codegen_supported<
							   ch_type,
							   ::fast_io::details::compiler_constant_materialized_t<
								   ch_type, Args>...>())
	{
		// A rejected code-generation proof must be decided before the value query; a true query may never fall back.
		return false;
	}
	else
	{
		using source_types =
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_type_list<
					::std::remove_cvref_t<Args>...>;
		using replacement_types =
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_type_list<
					::fast_io::details::compiler_constant_materialized_t<
						ch_type, Args>...>;
		// Direct replacement is unavailable when either spelling would invoke a whole-run destination status owner.
		if constexpr (!::fast_io::details::decay::
						  basic_general_concat_compiler_constant_status_safe_for_types<
							  line, ch_type, T, source_types, replacement_types>())
		{
			return false;
		}

		constexpr ::std::size_t reserve_size{
			::fast_io::operations::decay::
				print_compiler_constant_materialization_reserve_size<
					line, ch_type, Args...>()};
		constexpr ::std::size_t proxy_bytes{
			::fast_io::operations::decay::
				print_compiler_constant_materialization_proxy_bytes<
					ch_type, Args...>()};
		constexpr bool compact_reserve_plan{
			reserve_size != SIZE_MAX &&
			reserve_size <=
				::fast_io::details::compiler_constant_materialization_max_bytes /
					sizeof(ch_type)};
		constexpr ::std::size_t retained_reserve_size{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes /
				sizeof(ch_type)};
			::std::size_t total{};
			// The enclosing immediate proof owns evaluation.  A constexpr leaf
			// also accepts its evolving accumulator on pre-DR20 Clang 13--15.
			((total = [](::std::size_t current) constexpr {
				 using source_type = ::std::remove_cvref_t<Args>;
				 using replacement_type =
					 ::fast_io::details::compiler_constant_materialized_t<
						 ch_type, Args>;
				 if constexpr (::std::same_as<source_type, replacement_type>)
				 {
					 constexpr ::std::size_t extent{print_reserve_size(
						 ::fast_io::io_reserve_type<ch_type, replacement_type>)};
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
		constexpr bool exact_destination_plan{
			::fast_io::details::decay::
				basic_general_concat_precise_resize_destination_run_v<
					ch_type, T,
					::fast_io::details::compiler_constant_materialized_t<
						ch_type, Args>...>};
		return proxy_bytes != SIZE_MAX &&
			   proxy_bytes <=
				   ::fast_io::details::compiler_constant_materialization_max_bytes &&
			   (compact_reserve_plan ||
				(exact_destination_plan && retained_reserve_size != SIZE_MAX)) &&
			   (false || ... ||
				!::std::same_as<
					::std::remove_cvref_t<Args>,
					::fast_io::details::compiler_constant_materialized_t<ch_type, Args>>);
	}
}

/// @brief Emits one exact-preferred compiler-constant replacement into a fresh writable concat result.
/// @details This is the narrow consumer promised by `compiler_constant_precise_compact_preferred`: it queries the exact
///          size once, establishes that complete destination extent, invokes only the precise writer, verifies a
///          pointer-reporting endpoint, and publishes the cursor once. The ordinary reserve writer is intentionally not
///          named. GCC 12--17 otherwise outline this proxy-only bridge and leave it reachable from the successful
///          public concat symbol; GCC 11 is rejected by the availability proof above. Clang 21--23 have the same
///          measured proxy boundary. Native run-time scalar types cannot satisfy the constraint.
template <bool line, ::std::integral ch_type, typename T, typename Arg>
	requires(
		::fast_io::buffer_strlike<ch_type, T> &&
		::fast_io::compiler_constant_precise_compact_preferred<ch_type, Arg>)
#if (defined(__GNUC__) && !defined(__clang__) && 12 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr void
	basic_general_concat_compiler_constant_decay_ref_impl_precise(
		T &result, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const payload_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<ch_type, arg_type>, arg)};
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(
			payload_size)};
	if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_capacity{
			strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
		if (local_capacity < total_size)
		{
			strlike_reserve(
				::fast_io::io_strlike_type<ch_type, T>, result, total_size);
		}
	}
	else
	{
		strlike_reserve(
			::fast_io::io_strlike_type<ch_type, T>, result, total_size);
	}
	ch_type *const first{
		strlike_begin(::fast_io::io_strlike_type<ch_type, T>, result)};
	ch_type *const expected_payload_end{first + payload_size};
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<ch_type, arg_type>, first, payload_size,
		arg));
	if constexpr (::std::same_as<define_result, ch_type *>)
	{
		ch_type *const actual_end{print_reserve_precise_define(
			::fast_io::io_reserve_type<ch_type, arg_type>, first,
			payload_size, arg)};
		if (actual_end != expected_payload_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	else
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<ch_type, arg_type>, first,
			payload_size, arg);
	}
	ch_type *published_end{expected_payload_end};
	if constexpr (line)
	{
		*published_end++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	strlike_set_curr(
		::fast_io::io_strlike_type<ch_type, T>, result, published_end);
}

/// @brief Constructs a concat result from an already-proved compiler-constant replacement pack.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// GCC 11--16 otherwise separates the already-proved proxy pack from result
// construction.  The complete deletion matrix requires this edge together
// with the local fold below; it also reduces every tested unknown-value facade
// aggregate, so this is not a constant win bought with runtime text growth.
// On Clang 21--23, deleting this builder alone from the complete condition
// chain restores a reachable proxy/native formatter graph. Clang 16--20 fail
// the complete candidate and grow text, so they deliberately remain unforced.
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_compiler_constant_materialized(Args... args)
{
	constexpr ::std::size_t reserve_size{
		::fast_io::details::decay::
			calculate_concat_scatter_reserve_size_or_unavailable<
				ch_type, Args...>()};
	constexpr ::std::size_t reserve_size_with_line{
		::fast_io::details::decay::
			print_contiguous_char_extent_add_or_unavailable<ch_type>(
				reserve_size, static_cast<::std::size_t>(line))};
	constexpr bool compact_reserve_plan{
		reserve_size_with_line != SIZE_MAX &&
		reserve_size_with_line <=
			::fast_io::details::compiler_constant_materialization_max_bytes /
				sizeof(ch_type)};
	constexpr bool precise_compact_preferred{
		sizeof...(Args) == 1u &&
		(::fast_io::compiler_constant_precise_compact_preferred<
			 ch_type, Args> &&
		 ...)};
	if constexpr (
		precise_compact_preferred &&
		::fast_io::buffer_strlike<ch_type, T>)
	{
		// The replacement marker promises an exact cheap spelling. Substituting its conservative ordinary reserve writer
		// would discard that provider proof and may instantiate the native fallback after the constant gate succeeded.
		T result;
		(::fast_io::details::decay::
			 basic_general_concat_compiler_constant_decay_ref_impl_precise<
				 line, ch_type, T>(result, args),
		 ...);
		return result;
	}
	else if constexpr (
		(precise_compact_preferred || !compact_reserve_plan) &&
		::fast_io::details::decay::
			basic_general_concat_precise_resize_destination_run_v<
				ch_type, T, Args...>)
	{
		// Compiler-constant replacement is an allocation strategy, not a scatter-output strategy.  When the replacement
		// run exposes exact sizes and the destination can establish one writable logical extent, write every proxy
		// directly into that extent.  This is also the bounded escape hatch for compact float proxy state whose mature
		// reserve capacity is intentionally much larger than the constant-materialization byte budget.
		return ::fast_io::details::decay::basic_general_concat_precise_resize_run<
			line, ch_type, T>(args...);
	}
	else if constexpr (precise_compact_preferred)
	{
		// A construct-only result has no exact writable extent. Materialize the proved exact proxy in concat's owned
		// staging string, then construct the destination from the completed range. The ordinary reserve writer remains
		// forbidden for this marker even though its conservative capacity happens to fit the byte budget.
		::fast_io::details::basic_concat_buffer<ch_type> buffer;
		(::fast_io::details::decay::
			 basic_general_concat_compiler_constant_decay_ref_impl_precise<
				 line, ch_type>(buffer, args),
		 ...);
		return strlike_construct_define(
			::fast_io::io_strlike_type<ch_type, T>, buffer.buffer_begin,
			buffer.buffer_curr);
	}
	else if constexpr (compact_reserve_plan &&
					   ::fast_io::buffer_strlike<ch_type, T>)
	{
		// Keep the selected proxy run contiguous.  Re-entering the complete phase-1 dispatcher here is both unnecessary
		// (the gate has already proved an all-reserve run) and can make a large caller outline the value-sensitive writer.
		// A writable string result instead establishes its one destination capacity and receives every replacement in
		// order.  No static-fragment/scatter output protocol participates in concat.
		T result;
		if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
		{
			constexpr ::std::size_t local_capacity{
				strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
			if constexpr (local_capacity < reserve_size_with_line)
			{
				strlike_reserve(
					::fast_io::io_strlike_type<ch_type, T>, result,
					reserve_size_with_line);
			}
		}
		else if constexpr (reserve_size_with_line != 0u)
		{
			strlike_reserve(
				::fast_io::io_strlike_type<ch_type, T>, result,
				reserve_size_with_line);
		}
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
		ch_type *end{strlike_begin(
			::fast_io::io_strlike_type<ch_type, T>, result)};
		((end = print_reserve_define(
			  io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>, end, args)),
		 ...);
		// The line specialization appends its terminator inside the same exact destination extent.
		if constexpr (line)
		{
			*end++ = char_literal_v<u8'\n', ch_type>;
		}
#else
		ch_type *const end{print_reserve_define_chain_impl<line>(
			strlike_begin(::fast_io::io_strlike_type<ch_type, T>, result),
			args...)};
#endif
		strlike_set_curr(
			::fast_io::io_strlike_type<ch_type, T>, result, end);
		return result;
	}
	else if constexpr (
		compact_reserve_plan &&
		::fast_io::range_constructible_strlike<ch_type, T>)
	{
		// Construct-only strings cannot expose their final storage in C++20.  The bounded local range is nevertheless a
		// single contiguous materialization, and the range constructor owns the sole destination allocation/copy.  Short
		// standard-string results are normally scalar-replaced into direct SSO stores by both supported compilers.
		ch_type buffer[reserve_size_with_line == 0u ? 1u : reserve_size_with_line];
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
		ch_type *end{buffer};
		((end = print_reserve_define(
			  io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>, end, args)),
		 ...);
		// The bounded construction includes one extra element only for the line-terminating specialization.
		if constexpr (line)
		{
			*end++ = char_literal_v<u8'\n', ch_type>;
		}
#else
		ch_type *const end{print_reserve_define_chain_impl<line>(buffer, args...)};
#endif
		return strlike_construct_define(
			::fast_io::io_strlike_type<ch_type, T>, buffer, end);
	}
	else
	{
		// Uncommon construct-only adapters retain the mature phase-1 continuation.  This branch is never selected solely
		// from a large compiler-constant reserve bound: that case requires the exact-resize destination proof above.
		return ::fast_io::details::decay::
			basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(args...);
	}
}

/// @brief Proves that concat may query the exact active source shapes on this compiler.
/// @details Recursive assembly and IR audits show that Clang 13--20 retain integer replacement helpers, and Clang 14--20
///          also retain timestamp proxies. Those releases are therefore rejected as complete source records before any
///          replacement type is formed. Clang 21--23 fully erase integer, timestamp, ordinary floating, and simple
///          condition records, but retain both the exact-precision planner and native formatter for dynamic-precision
///          floating sources. This source-only predicate is shared by normalized and specialized condition consumers
///          without inheriting either consumer's later replacement policy. A borrowed-text spelling is rejected on every
///          compiler: its public source gate already classifies it as passive, but a format wrapper survives ordinary
///          normalization and reaches this later gate unchanged. Repeating the type-only rejection here prevents that
///          normalized record from reopening a query whose proxy would merely copy the same borrowed range.
template <::std::integral ch_type, typename... Args>
inline consteval bool
basic_general_concat_active_source_codegen_supported() noexcept
{
	if constexpr (
		(false || ... ||
		 ::fast_io::compiler_constant_borrowed_text_source_shape<
			 ch_type, Args>))
	{
		return false;
	}
#if defined(__clang__) && __clang_major__ < 21
	else
	{
		return false;
	}
#elif defined(__clang__)
	else
	{
		return !(false || ... ||
				 ::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
					 ch_type, Args>);
	}
#else
	else
	{
		return true;
	}
#endif
}

/// @brief Proves that the post-decay concat gate can erase a precise-preferred replacement graph.
/// @details This proof is deliberately narrower than source admission. GCC 12 and GCC 13 retain the precise-preferred
///          true arm when the same query is instantiated in the later normalized phase-1 helper.
///          On GCC 12, paired object analysis found two 15,043/14,793-byte helpers; rejecting only this late query
///          removed 30,154 text bytes without changing any focused constant, run-time, or volatile caller. On GCC 13,
///          the brace-format constant caller remained byte-identical and its run-time peer became proxy-clean while
///          aggregate text fell by 13,654 bytes. The run-time wrapper itself grew from 0x1b to 0x30 bytes because it now
///          passes the normalized record by reference, but the hidden query/proxy helper disappeared, so this local
///          0x15-byte cost is not an inlining justification. GCC 11 admits the proved flat integer only at the public
///          source gate and rejects its duplicate post-decay query; GCC 14 and later retain the late optimization until
///          a measured reversal.
///          The independent source-shape proof above rejects Clang dynamic-precision leaves while preserving timestamp
///          and flat-scalar paths. Later consumers apply their replacement-specific proofs here.
///          The printable guard is evaluated first so an unsupported leaf never forms its materialized replacement type.
template <::std::integral ch_type, typename... Args>
inline consteval bool
basic_general_concat_normalized_compiler_constant_codegen_supported() noexcept
{
	if constexpr (!::fast_io::details::decay::
					  basic_general_concat_active_source_codegen_supported<
						  ch_type, Args...>())
	{
		return false;
	}
	else if constexpr (
		!(::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			  ch_type, Args> &&
		  ...))
	{
		// This normalized consumer may not infer deletion merely from semantic printability. Reject the whole active
		// record before a replacement type or optimizer query is formed.
		return false;
	}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 11
	else if constexpr (!(::fast_io::compiler_constant_printable<
							 ch_type, Args> &&
						 ...))
	{
		return true;
	}
	else
	{
		// GCC 11 proves the flat public source gate below but perturbs the historical unknown continuation if the same
		// query is repeated after decay. Keep this later consumer closed for that exact replacement.
		return !(false || ... ||
				 ::fast_io::compiler_constant_flat_integer_replacement<
					 ch_type,
					 ::fast_io::details::compiler_constant_materialized_t<
						 ch_type, Args>>);
	}
#elif defined(__GNUC__) && !defined(__clang__) && 12 <= __GNUC__ && __GNUC__ <= 13
	else if constexpr (!(::fast_io::compiler_constant_printable<ch_type, Args> && ...))
	{
		return true;
	}
	else
	{
		return !(false || ... ||
				 ::fast_io::compiler_constant_precise_compact_preferred<
					 ch_type,
					 ::fast_io::details::compiler_constant_materialized_t<
						 ch_type, Args>>);
	}
#else
	else
	{
		return true;
	}
#endif
}

/// @brief Keeps concat's original phase-1 body as the sole unknown-value continuation.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
// Condition selection has now produced the exact active IO record.  This is
// therefore the first semantically valid point at which a dynamic-star printf
// leaf may be queried for compiler constancy.  Outlining this function makes
// every source opaque and retains both the constant proxy and runtime formatter;
// forced placement affects only the query boundary, while its false arm remains
// the original phase-1 concat body below.
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_compiler_constant_dispatch(Args &...args)
{
	if constexpr (
		::fast_io::details::decay::
			basic_general_concat_normalized_compiler_constant_codegen_supported<
				ch_type, Args...>() &&
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_materialization_available<
				line, ch_type, T, Args...>())
	{
		if (::fast_io::operations::decay::
				print_compiler_constant_materialization_gate<ch_type>(args...))
		{
			return ::fast_io::details::decay::
				basic_general_concat_compiler_constant_materialized<
					line, ch_type, T>(
					print_compiler_constant_materialize_gate_proven(
						::fast_io::io_reserve_type<ch_type,
												   ::std::remove_cvref_t<Args>>,
						args)...);
		}
	}
	return ::fast_io::details::decay::
		basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(args...);
}

/// @brief Owns a structurally flat normalized run exactly once before all strategy helpers borrow it.
/// @details The direct public path supplies prvalue normalization results, so this boundary materializes their compact
///          decayed types and then exposes only named lvalues. Pack expansion and condition selection already own any
///          value-producing customization in their synchronous continuation frames and therefore bypass this boundary;
///          copying those selected lvalues here would be a second decay and would reject valid move-only transports.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T
basic_general_concat_phase1_decay_impl(Args... args)
{
	return ::fast_io::details::decay::basic_general_concat_compiler_constant_dispatch<line, ch_type, T>(
		args...);
}

/// @brief Models one source argument after the optional pre-normalization replacement and concat's ordinary flat
///        alias/status forwarding.
template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_replacement_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_replacement_t<ch_type, T>;

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_alias_t = decltype(::fast_io::io_print_alias(::std::declval<
																								 ::fast_io::details::decay::
																									 basic_general_concat_compiler_constant_source_replacement_t<
																										 ch_type, T>>()));

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_forward_t = decltype(::fast_io::io_print_forward<ch_type>(::std::declval<
																											  ::fast_io::details::decay::
																												  basic_general_concat_compiler_constant_source_alias_t<
																													  ch_type, T>>()));

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_normalized_t =
	::std::remove_cvref_t<::fast_io::details::decay::
							  basic_general_concat_compiler_constant_source_forward_t<ch_type, T>>;

/// @brief Models the normalized type seen by concat's historical public-source arm.
/// @details Unlike print's named-lvalue bridge, concat preserves each public argument's forwarding category through
///          alias and status forwarding.  The compiler-constant safety proof must model that exact expression: using a
///          named lvalue here could miss an rvalue-qualified whole-run status customization and let replacement change
///          observable output.
template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_historical_alias_t = decltype(::fast_io::io_print_alias(::std::declval<T>()));

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_historical_normalized_t =
	::std::remove_cvref_t<decltype(::fast_io::io_print_forward<ch_type>(
		::std::declval<::fast_io::details::decay::
						   basic_general_concat_compiler_constant_historical_alias_t<
							   ch_type, T>>()))>;

/// @brief Rejects a compiler-constant replacement which could bypass a whole-run status owner selected by concat.
/// @details Compiler-constant concat constructs its result directly, so it cannot preserve a destination-specific
///          `status_print_define` by entering the ordinary output dispatcher.  Check both complete normalized packs on
///          every physical destination concat may use: the associated result adapter, the maintained writable-buffer
///          adapter, or the internal construction buffer.  The shared proof recursively unwraps a complete mutex
///          protocol.  Allowing a staged obuffer here disables only print's put-area profitability restriction; it does
///          not weaken the whole-run status check which this predicate needs.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_compiler_constant_source_status_safe() noexcept
{
	using source_types =
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_type_list<
				::fast_io::details::decay::
					basic_general_concat_compiler_constant_historical_normalized_t<
						ch_type, Args>...>;
	using replacement_types =
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_type_list<
				::fast_io::details::decay::
					basic_general_concat_compiler_constant_source_normalized_t<
						ch_type, Args>...>;
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_status_safe_for_types<
			line, ch_type, T, source_types, replacement_types>();
}

/// @brief Completes source availability after the destination/source code-generation pair has been proved.
/// @details This is intentionally a flat-run proof. Semantic packs, conditions, and value-owning transports retain the
///          established normalization graph and may still use the normalized phase-1 gate. For a flat run, every
///          replacement is normalized exactly as the true-arm expression below, every resulting leaf must have a
///          contiguous reserve protocol, and the compact proxy state remains independently bounded. A floating proxy's
///          mature 5006-character reserve capacity is admitted only when all leaves expose exact sizes and the result
///          can establish the corresponding live destination range.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_compiler_constant_source_available_after_codegen_supported() noexcept
{
	constexpr bool has_candidate{
		(false || ... ||
		 ::fast_io::operations::decay::
			 print_compiler_constant_pre_normalization_candidate_v<
				 ch_type, Args>)};
	constexpr bool source_semantic_run{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 ch_type, Args>)};
	constexpr bool replacement_semantic_run{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 ch_type,
			 ::fast_io::details::decay::
				 basic_general_concat_compiler_constant_source_replacement_t<
					 ch_type, Args>>)};
	// Semantic graphs retain their graph-owned normalization, and an empty candidate set has no replacement work.
	if constexpr (!has_candidate || source_semantic_run || replacement_semantic_run)
	{
		return false;
	}
	// Preserve any whole-run status customization before admitting the compiler-constant source replacement.
	else if constexpr (!::fast_io::details::decay::
						   basic_general_concat_compiler_constant_source_status_safe<
							   line, ch_type, T, Args...>())
	{
		return false;
	}
	// A compiler-specific rejection belongs before the source query for the same reason as the normalized gate: after a
	// true query, concat may not recover by invoking the native formatter.
	else if constexpr (!::fast_io::details::decay::
						   basic_general_concat_compiler_constant_replacement_codegen_supported<
							   ch_type,
							   ::fast_io::details::decay::
								   basic_general_concat_compiler_constant_source_normalized_t<
									   ch_type, Args>...>())
	{
		return false;
	}
	// Every normalized replacement must expose one contiguous reserve spelling before direct construction is legal.
	else if constexpr (!(::fast_io::reserve_printable<
							 ch_type,
							 ::fast_io::details::decay::
								 basic_general_concat_compiler_constant_source_normalized_t<
									 ch_type, Args>> &&
						 ...))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t maximum_bytes{
			::fast_io::details::compiler_constant_materialization_max_bytes};
		constexpr ::std::size_t proxy_bytes{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes};
			::std::size_t total{};
			((total = total > maximum || sizeof(::fast_io::details::decay::
													basic_general_concat_compiler_constant_source_normalized_t<
														ch_type, Args>) > maximum - total
						  ? SIZE_MAX
						  : total + sizeof(::fast_io::details::decay::
											   basic_general_concat_compiler_constant_source_normalized_t<
												   ch_type, Args>)),
			 ...);
			return total;
		}()};
		constexpr ::std::size_t reserve_size{
			::fast_io::details::decay::
				calculate_concat_scatter_reserve_size_or_unavailable<
					ch_type,
					::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>...>()};
		constexpr ::std::size_t reserve_size_with_line{
			::fast_io::details::decay::
				print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
		constexpr bool compact_reserve_plan{
			reserve_size_with_line != SIZE_MAX &&
			reserve_size_with_line <= maximum_bytes / sizeof(ch_type)};
		constexpr ::std::size_t retained_reserve_size{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes /
				sizeof(ch_type)};
			::std::size_t total{};
			// The enclosing immediate proof owns evaluation.  A constexpr leaf
			// also accepts its evolving accumulator on pre-DR20 Clang 13--15.
			((total = [](::std::size_t current) constexpr {
				 if constexpr (!::fast_io::operations::decay::
								   print_compiler_constant_pre_normalization_candidate_v<
									   ch_type, Args>)
				 {
					 using normalized_type =
						 ::fast_io::details::decay::
							 basic_general_concat_compiler_constant_source_normalized_t<
								 ch_type, Args>;
					 constexpr ::std::size_t extent{print_reserve_size(
						 ::fast_io::io_reserve_type<ch_type, normalized_type>)};
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
		constexpr bool exact_destination_plan{
			::fast_io::details::decay::
				basic_general_concat_precise_resize_destination_run_v<
					ch_type, T,
					::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>...>};
		return proxy_bytes != SIZE_MAX && proxy_bytes <= maximum_bytes &&
			   (compact_reserve_plan ||
				(exact_destination_plan && retained_reserve_size != SIZE_MAX));
	}
}

/// @brief Proves independently that concat may query and replace one public flat source run.
/// @details The destination/source code-generation partition is evaluated before the implementation above can name a
///          replacement type. This makes the availability predicate itself fail closed, so future consumers cannot
///          accidentally bypass the checked-entry short circuit and instantiate a rejected replacement graph.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_compiler_constant_source_available() noexcept
{
	if constexpr (!::fast_io::details::decay::
					  basic_general_concat_compiler_constant_source_codegen_supported<
						  ch_type, T, Args...>())
	{
		return false;
	}
	else
	{
		return ::fast_io::details::decay::
			basic_general_concat_compiler_constant_source_available_after_codegen_supported<
				line, ch_type, T, Args...>();
	}
}

template <bool line, ::std::integral ch_type, typename T,
		  typename... ReplacedArgs>
inline constexpr T
basic_general_concat_compiler_constant_source_normalized(
	ReplacedArgs &&...args)
{
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_materialized<line, ch_type, T>(
			::fast_io::io_print_forward<ch_type>(::fast_io::io_print_alias(
				::std::forward<ReplacedArgs>(args)))...);
}

/**
 * Completes a compact source replacement without re-entering the recursive
 * reserve chain.
 *
 * The source gate has already proved a flat reserve-printable replacement
 * pack, its byte budget, destination operation, and whole-run status safety.
 * Keeping the selected compact branch here makes the replacement values and
 * final stores one optimizer unit; the recursive generic chain otherwise
 * outlines after the integer-field floating proxy has been formed.  A
 * noncompact replacement delegates to the established exact-size/fallback
 * dispatcher unchanged.
 *
 * In a complete 2^3 deletion matrix Clang 21--23 require both this compact edge
 * and the source edge below.  Clang 17--20 are deliberately excluded because
 * their constant-float emitter has the opposite code-size result.  The latest
 * tested positive version keeps the interval open for newer frontends.  GCC
 * uses the smaller direct materialized path above instead of instantiating this
 * extra source-only helper.
 */
template <bool line, ::std::integral ch_type, typename T,
		  typename... NormalizedArgs>
#if defined(__clang__) && 21 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_compiler_constant_compact_source_run(
		NormalizedArgs... args)
{
	constexpr ::std::size_t reserve_size{
		::fast_io::details::decay::
			calculate_concat_scatter_reserve_size_or_unavailable<
				ch_type, NormalizedArgs...>()};
	constexpr ::std::size_t reserve_size_with_line{
		::fast_io::details::decay::
			print_contiguous_char_extent_add_or_unavailable<ch_type>(
				reserve_size, static_cast<::std::size_t>(line))};
	constexpr bool compact_reserve_plan{
		reserve_size_with_line != SIZE_MAX &&
		reserve_size_with_line <=
			::fast_io::details::compiler_constant_materialization_max_bytes /
				sizeof(ch_type)};

	// A compact proxy run writes directly only when the fresh result exposes a writable string buffer.
	if constexpr (compact_reserve_plan &&
				  ::fast_io::buffer_strlike<ch_type, T>)
	{
		T result;
		// SSO capacity is a compile-time reserve decision and avoids an unnecessary heap request for short records.
		if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
		{
			constexpr ::std::size_t local_capacity{
				strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
			// Grow only when the exact constant record cannot fit in the destination's local storage.
			if constexpr (local_capacity < reserve_size_with_line)
			{
				strlike_reserve(
					::fast_io::io_strlike_type<ch_type, T>, result,
					reserve_size_with_line);
			}
		}
		// Non-SSO buffers need no allocation request for an empty non-line record.
		else if constexpr (reserve_size_with_line != 0u)
		{
			strlike_reserve(
				::fast_io::io_strlike_type<ch_type, T>, result,
				reserve_size_with_line);
		}
		ch_type *end{strlike_begin(
			::fast_io::io_strlike_type<ch_type, T>, result)};
		((end = print_reserve_define(
			  io_reserve_type<ch_type, NormalizedArgs>, end, args)),
		 ...);
		// The newline belongs to the exact compact record and is committed with the replacement payload.
		if constexpr (line)
		{
			*end++ = char_literal_v<u8'\n', ch_type>;
		}
		strlike_set_curr(
			::fast_io::io_strlike_type<ch_type, T>, result, end);
		return result;
	}
	// Construct-only results use one bounded local range when no writable destination cursor is available.
	else if constexpr (
		compact_reserve_plan &&
		::fast_io::range_constructible_strlike<ch_type, T>)
	{
		ch_type buffer[reserve_size_with_line == 0u ? 1u : reserve_size_with_line];
		ch_type *end{buffer};
		((end = print_reserve_define(
			  io_reserve_type<ch_type, NormalizedArgs>, end, args)),
		 ...);
		// Match the precomputed line-inclusive capacity before passing the completed range to the constructor.
		if constexpr (line)
		{
			*end++ = char_literal_v<u8'\n', ch_type>;
		}
		return strlike_construct_define(
			::fast_io::io_strlike_type<ch_type, T>, buffer, end);
	}
	else
	{
		return ::fast_io::details::decay::
			basic_general_concat_compiler_constant_materialized<
				line, ch_type, T>(args...);
	}
}

/// @brief Materializes public compiler-constant sources through the compiler-specific compact construction edge.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
// A strict per-symbol precision gate supersedes the earlier scalar-only result:
// GCC 11--16 outline this true-only source edge after forming the precision
// proxy, which leaves a compiler-constant helper call in the public concat
// symbol. Clang 21--23 require the same edge together with the compact edge
// above. Unknown sources cannot enter this function because the public builtin
// query is evaluated before it is called.
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_compiler_constant_source_materialized(Args &&...args)
{
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_materialized<line, ch_type, T>(
			::fast_io::io_print_forward<ch_type>(::fast_io::io_print_alias(
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_materialize_one<
						ch_type>(::std::forward<Args>(args))))...);
#elif defined(__clang__) && 21 <= __clang_major__
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_compact_source_run<
			line, ch_type, T>(
			::fast_io::io_print_forward<ch_type>(::fast_io::io_print_alias(
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_materialize_one<
						ch_type>(::std::forward<Args>(args))))...);
#else
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_source_normalized<
			line, ch_type, T>(
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_materialize_one<
					ch_type>(::std::forward<Args>(args))...);
#endif
}

/// @brief Enters concat sizing only after semantic structure has been normalized.
/// @details Phase 1 assumes that every argument is an active printable leaf. Keeping that assumption behind a
///          continuation prevents an inactive condition branch from contributing reserve capacity or selecting a
///          materialization strategy. Pack expansion and condition selection invoke this continuation synchronously,
///          so all forwarded references remain valid through sizing and the immediately following write pass.
template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_normalized_phase1_continuation
{
	/// @brief Sizes and materializes the normalized leaf sequence.
	template <typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
	FAST_IO_GNU_ALWAYS_INLINE
#endif
		inline constexpr T operator()(Args &&...args) const
	{
		// Every argument is now a named object owned by an enclosing normalization/selection frame. The complete phase-1
		// call is nested synchronously inside that frame, so borrowing preserves lifetime while avoiding a second copy.
		return ::fast_io::details::decay::basic_general_concat_compiler_constant_dispatch<line, ch_type, T>(
			args...);
	}
};

/// @brief Selects conditions exposed by semantic pack expansion before concat phase 1.
/// @details A pack can contain conditions that were not visible in the original top-level argument list. The stored
///          pointer is non-owning but cannot escape: print_semantic_pack_expand completes the nested invocation before
///          returning, and the referenced continuation outlives that complete call chain.
template <::std::integral ch_type, typename continuation>
struct basic_general_concat_select_conditions_continuation
{
	::std::remove_reference_t<continuation> *contptr;

	/// @brief Recursively removes inactive branches and forwards the active leaves to the saved continuation.
	template <typename... Args>
	inline constexpr decltype(auto) operator()(Args &&...args) const
	{
		return ::fast_io::details::decay::print_semantic_select_conditions<ch_type>(
			*contptr, ::std::forward<Args>(args)...);
	}
};

/// @brief Proves that one selected condition arm can own concat's value query at the active-record boundary.
/// @details The arm must normalize to one ordinary non-null leaf whose query is explicitly safe to evaluate in its
///          caller.  The existing materialization proof independently checks the source/replacement status protocols,
///          proxy budget, and every physical destination which concat may select.  Reusing that complete proof keeps
///          this source-shape specialization from turning format lowering into a concat policy decision.
template <bool line, ::std::integral ch_type, typename T, typename Branch>
inline consteval bool
basic_general_concat_single_condition_active_branch_available() noexcept
{
	using active_result = decltype(::fast_io::details::decay::print_semantic_input_forward<ch_type>(
		::std::declval<Branch>()));
	using active_type = ::std::remove_cvref_t<active_result>;
	if constexpr (
		::fast_io::details::decay::
			print_semantic_execution_node_v<active_result> ||
		::std::same_as<active_type, ::fast_io::io_null_t>)
	{
		return false;
	}
	else if constexpr (!::fast_io::details::decay::
						   basic_general_concat_active_source_codegen_supported<
							   ch_type, active_type>())
	{
		return false;
	}
	else if constexpr (!::fast_io::operations::decay::
						   print_compiler_constant_pre_normalization_candidate_v<
							   ch_type, active_result>)
	{
		return false;
	}
	else
	{
		using replacement_type =
			::fast_io::details::compiler_constant_materialized_t<
				ch_type, active_type>;
		constexpr bool codegen_supported{
			::fast_io::details::decay::
				basic_general_concat_compiler_constant_replacement_codegen_supported<
					ch_type, replacement_type>()};
		if constexpr (!codegen_supported)
		{
			// A rejected replacement still benefits from this one-leaf condition selector. It enters only the historical
			// phase-1 continuation below, so no query, proxy, or replacement-status proof is required or evaluated.
			return true;
		}
		else
		{
			return ::fast_io::compiler_constant_query_inline_safe<
					   ch_type, active_type> &&
				   ::fast_io::details::decay::
					   basic_general_concat_compiler_constant_materialization_available<
						   line, ch_type, T, active_type>();
		}
	}
}

/// @brief Proves the exact one-condition source shape whose active concat record can be queried before generic phase 1.
/// @details Source normalization is modeled with the same forwarding CPO used by the implementation.  Both alternatives
///          must independently satisfy the complete active-leaf proof because the run-time predicate is not a type-level
///          fact.  Packs, nested conditions, null alternatives, volatile sources, and multi-source records fail closed
///          and retain recursive semantic normalization.
template <bool line, ::std::integral ch_type, typename T, typename Source>
inline consteval bool
basic_general_concat_single_condition_active_record_available_one() noexcept
{
	if constexpr (::std::is_volatile_v<::std::remove_reference_t<Source>>)
	{
		return false;
	}
	else
	{
		using normalized_result = decltype(::fast_io::details::decay::print_semantic_input_forward<ch_type>(
			::std::declval<Source>()));
		using normalized_expression = ::std::add_lvalue_reference_t<
			::std::remove_reference_t<normalized_result>>;
		if constexpr (!::fast_io::details::decay::
						  print_semantic_top_level_condition_v<normalized_expression>)
		{
			return false;
		}
		else
		{
			using node_reference = decltype(::fast_io::details::decay::print_semantic_node_ref(
				::std::declval<normalized_expression>()));
			using named_node_reference = ::std::add_lvalue_reference_t<
				::std::remove_reference_t<node_reference>>;
			using first_arm_reference = decltype((::std::declval<named_node_reference>().t1));
			using second_arm_reference = decltype((::std::declval<named_node_reference>().t2));
			return ::fast_io::details::decay::
					   basic_general_concat_single_condition_active_branch_available<
						   line, ch_type, T, first_arm_reference>() &&
				   ::fast_io::details::decay::
					   basic_general_concat_single_condition_active_branch_available<
						   line, ch_type, T, second_arm_reference>();
		}
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_single_condition_active_record_available() noexcept
{
	if constexpr (sizeof...(Args) != 1u)
	{
		return false;
	}
	else
	{
		return (false || ... ||
				::fast_io::details::decay::
					basic_general_concat_single_condition_active_record_available_one<
						line, ch_type, T, Args>());
	}
}

/// @brief Executes one selected concat arm with mutually exclusive constant and run-time continuations.
/// @details Forwarding occurs once.  A successful query constructs the replacement and enters concat's already-proved
///          contiguous result builder; the rejected query passes the same named active leaf directly to the historical
///          phase-1 body.  The false arm therefore constructs no proxy and the true arm has no edge back to the native
///          formatter. Clang deliberately leaves this branch ordinary inline: forcing it reduced 214--242 text bytes
///          and 30 root instructions but slowed the paired run-time concat geomean by 4.67--5.52%, with one case losing
///          10.04%. The smaller graph is therefore not a profitability proof.
template <bool line, ::std::integral ch_type, typename T, typename Branch>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_single_condition_active_branch(Branch &&branch)
{
	decltype(auto) active{
		::fast_io::details::decay::print_semantic_input_forward<ch_type>(
			::std::forward<Branch>(branch))};
	if constexpr (
		::fast_io::details::decay::
			basic_general_concat_active_source_codegen_supported<
				ch_type, ::std::remove_cvref_t<decltype(active)>>() &&
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_materialization_available<
				line, ch_type, T, ::std::remove_cvref_t<decltype(active)>>())
	{
		if (::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_gate<ch_type>(active))
		{
			return ::fast_io::details::decay::
				basic_general_concat_compiler_constant_materialized<
					line, ch_type, T>(
					::fast_io::operations::decay::
						print_compiler_constant_active_record_materialize_one<
							ch_type>(active));
		}
	}
	return ::fast_io::details::decay::
		basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(active);
}

/// @brief Normalizes one concat condition and evaluates the optimizer query on the selected IO-level leaf.
/// @details The format layer may create the condition node, but it contributes no strategy here.  Selection, status
///          proof, replacement, result allocation, and the native run-time continuation are all concat/IO policies.
///          A recursive deletion matrix proves this record selector jointly necessary on Clang 21--23: deleting it
///          alone restores the forbidden proxy/native graph from a successful constant root. This proof does not apply
///          to the branch helper above, whose independent paired benchmark rejects forced placement.
template <bool line, ::std::integral ch_type, typename T, typename Source>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_single_condition_active_record(Source &&source)
{
	static_assert(
		::fast_io::details::decay::
			basic_general_concat_single_condition_active_record_available_one<
				line, ch_type, T, Source>());
	decltype(auto) normalized_condition{
		::fast_io::details::decay::print_semantic_input_forward<ch_type>(
			::std::forward<Source>(source))};
	auto &&node_ref{
		::fast_io::details::decay::print_semantic_node_ref(
			normalized_condition)};
	if (node_ref.pred)
	{
		return ::fast_io::details::decay::
			basic_general_concat_single_condition_active_branch<
				line, ch_type, T>(node_ref.t1);
	}
	return ::fast_io::details::decay::
		basic_general_concat_single_condition_active_branch<
			line, ch_type, T>(node_ref.t2);
}

} // namespace fast_io::details::decay

namespace fast_io
{

/// @brief Concat overload for one formally proved active compiler-constant condition record.
/// @details Keeping this overload structurally separate prevents the generic pack/condition continuation graph from
///          sharing an out-of-line helper between literal and unknown values.  The selected active leaf owns the only
///          value query; all other source shapes retain the established general overload below.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires(
		strlike<char_type, T> &&
		::fast_io::details::decay::
			basic_general_concat_single_condition_active_record_available<
				line, char_type, T, Args && ...>())
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T basic_general_concat(Args &&...args)
{
	return (::fast_io::details::decay::
				basic_general_concat_single_condition_active_record<
					line, char_type, T>(::std::forward<Args>(args)),
			...);
}

template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires(
		strlike<char_type, T> &&
		!::fast_io::details::decay::
			basic_general_concat_single_condition_active_record_available<
				line, char_type, T, Args && ...>())
inline constexpr T basic_general_concat(Args &&...args)
{
	// Normalization order is part of the allocation proof: alias/forward first, expand packs and remove nulls, select
	// active conditions recursively, and only then allow phase 1 to compute the output capacity. Public argument value
	// categories are preserved through the first CPO pair; every continuation below is synchronous and therefore keeps
	// a returned proxy/value alive until phase 1 finishes.
	using normalize_continuation =
		::fast_io::details::decay::basic_general_concat_normalized_phase1_continuation<line, char_type, T>;
	constexpr bool has_pack_or_null{
		(false || ... ||
		 (::fast_io::details::decay::print_semantic_pack_argument_v<
			  ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>> ||
		  ::std::same_as<::std::remove_cvref_t<
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>>,
						 ::fast_io::io_null_t>))};
	constexpr bool has_condition{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_top_level_condition_v<
			 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>>)};
	if constexpr (has_pack_or_null)
	{
		// Pack expansion comes first because it may reveal nested conditions. The following continuation then selects
		// those conditions, ensuring that neither inactive leaves nor nulls influence concat's capacity or cost model.
		normalize_continuation normalize;
		return ::fast_io::details::decay::print_semantic_pack_expand<true, char_type>(
			::fast_io::details::decay::basic_general_concat_select_conditions_continuation<
				char_type, normalize_continuation>{__builtin_addressof(normalize)},
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
	else if constexpr (has_condition)
	{
		// Without packs, direct recursive condition selection can enter phase 1 immediately after choosing active arms.
		return ::fast_io::details::decay::print_semantic_select_conditions<char_type>(
			normalize_continuation{},
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
	else
	{
		// A structurally flat run retains the original direct instantiation and pays no continuation machinery.
		return ::fast_io::details::decay::basic_general_concat_phase1_decay_impl<line, char_type, T>(
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
}

/// @brief Validates and executes concat against every destination phase 1 may actually select.
/// @details Reserve/scatter protocols are destination-independent, but a direct `print_define` customization may be
///          constrained to one stream type. The former public wrappers probed a synthetic dummy stream and then called
///          phase 1 directly; this could both reject a valid string-specific leaf and bypass pack/condition
///          normalization. Writable-buffer results have one real fallback destination. Non-buffer results prefer their
///          append adapter when the complete normalized run is accepted and otherwise use `basic_concat_buffer` before
///          range construction. Testing exactly that disjunction keeps admission, strategy selection, and eventual ADL
///          calls consistent while the normalized front door preserves the public argument value categories.
/// @tparam line      true to append a newline
/// @tparam char_type destination character type
/// @tparam T         string-like result type
/// @tparam Args      public concat argument types
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline consteval bool basic_general_concat_checked_available() noexcept
{
	constexpr bool printable_to_result{
		::fast_io::details::decay::basic_general_concat_direct_destination_ok<
			line, char_type, T,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...> ||
		::fast_io::details::decay::basic_general_concat_generic_buffer_destination_ok<
			line, char_type, T,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...>};
	constexpr bool printable_to_staging{
		::fast_io::details::decay::basic_general_concat_staging_destination_ok<
			line, char_type,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...>};
	// Writable-buffer results always execute fallback on their own adapter. A non-buffer result instead has two
	// destination-correct strategies: direct append when supported, otherwise internal staging followed by construction.
	constexpr bool printable_to_selected_destination{
		printable_to_result || (!buffer_strlike<char_type, T> && printable_to_staging)};
	return printable_to_selected_destination;
}

/// @brief Historical checked concat continuation used verbatim by the false arm of the source constant gate.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline constexpr T basic_general_concat_checked(Args &&...args)
{
	constexpr bool printable_to_selected_destination{
		::fast_io::basic_general_concat_checked_available<
			line, char_type, T, Args...>()};
	if constexpr (printable_to_selected_destination)
	{
		return ::fast_io::basic_general_concat<line, char_type, T>(::std::forward<Args>(args)...);
	}
	else
	{
		static_assert(printable_to_selected_destination,
					  "one or more arguments are not printable to any destination selected by concat");
		return {};
	}
}

/// @brief Keeps the optimizer-visible constant query at concat's public source boundary.
/// @details Only an explicitly pre-normalization-safe flat source run can enter the replacement arm. The false arm is
///          the ordinary checked implementation above, so an unknown integer/floating value performs no proxy
///          construction, size query, or branch at run time. Format lowering reaches this same entry with literals and
///          fields already translated to print/concat leaves; it owns no independent materialization policy.
/// @note Choose the string operation by its destination and parsing semantics:
///       - `concat(args...)` constructs and returns a fresh string-like value. It materializes the printable record
///         directly through strlike construction/growth protocols and does not reinterpret the produced characters
///         with a scanner. This is the intended operation for printable-to-string materialization.
///       - `print(ostring_ref, args...)` sends the same printable record directly to an existing string-like output
///         adapter. It appends at the current output cursor; it does not clear the destination and performs no scan.
///         Prefer this form when the caller owns an existing destination and append semantics are desired. For an empty
///         or deliberately cleared destination it commonly provides the most direct reusable-storage path.
///       - `inplace_to(mnp::whole_get(str), args...)` first formats the arguments and then consumes all resulting code
///         units with the whole-input scanner. It replaces the logical contents of `str`, including when the printed
///         record contains whitespace. This is a print-to-scan conversion into an existing object, not an output-stream
///         spelling of concat, and its scan state/EOF processing may cost more than direct string output.
///       - `to<T>(args...)` constructs a new `T` and scans the formatted representation according to `T`'s ordinary
///         scan grammar. In particular, a default string scanner is token-oriented, so `to<std::string>("ab c")`
///         produces `"ab"`, unlike concat or whole-input scanning.
///       Although `to<decltype(mnp::whole_get(str))>(args...)` may appear to request concat-like whole-string
///       semantics, it is not a usable replacement. `whole_get(str)` is a scanner carrier bound to the existing object
///       `str`; `decltype` preserves only its type, while `to<T>` must construct a new `T`. The carrier contains a
///       reference member and therefore normally has no default constructor. Use concat for a new string, direct print
///       for appending to an existing string, and inplace_to with whole_get only when replacement through the generic
///       print/scan conversion model is specifically required.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
// This is the concat-level builtin-query boundary, not an implementation
// detail which may be outlined.  A GCC 11--16 paired symbol audit found both
// literal and unknown precision callers invoking this function out of line;
// the builtin then observes only opaque parameters and necessarily retains the
// wrong branch graph.  Clang 13--23 has the same source-visibility obligation.
// The false arm enters the unchanged checked concat implementation and never
// constructs a compiler-constant proxy.
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	inline constexpr T
	basic_general_concat_compiler_constant_checked_entry(Args &&...args)
{
	if constexpr (
		::fast_io::basic_general_concat_checked_available<
			line, char_type, T, Args...>())
	{
		if constexpr (
			::fast_io::details::decay::
				basic_general_concat_single_condition_active_record_available<
					line, char_type, T, Args &&...>())
		{
			// The checked source proof covers both alternatives, while the
			// specialized proof above covers exact active-record status and
			// replacement safety. Keep the builtin query in this public concat
			// boundary so a literal never becomes an opaque callee parameter.
			return (::fast_io::details::decay::
						basic_general_concat_single_condition_active_record<
							line, char_type, T>(::std::forward<Args>(args)),
					...);
		}
		else if constexpr (
			!::fast_io::details::decay::
				basic_general_concat_compiler_constant_source_codegen_supported<
					char_type, T, Args &&...>())
		{
			// This destination/source pair is closed before source availability can form or query a replacement.
		}
		else if constexpr (
			::fast_io::details::decay::
				basic_general_concat_compiler_constant_source_available<
					line, char_type, T, Args &&...>())
		{
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 11
			if constexpr (
				sizeof...(Args) == 1u &&
				(::fast_io::compiler_constant_flat_integer_replacement<
					 char_type,
					 ::fast_io::details::decay::
						 basic_general_concat_compiler_constant_source_normalized_t<
							 char_type, Args &&>> &&
				 ...))
			{
				// GCC 11 otherwise schedules the unknown continuation through a spill after admitting this new
				// exact-integer arm. Paired `%u` roots prove the cold edge restores its historical instructions while
				// the literal erases the native writer. The attribute is confined to the newly admitted graph, so
				// unrelated compiler-constant categories retain their independently audited schedules.
				if (::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_gate<
							char_type>(args...)) [[unlikely]]
				{
					return ::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_materialized<
							line, char_type, T>(
							::std::forward<Args>(args)...);
				}
			}
			else
#endif
			{
				if (::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_gate<
							char_type>(args...))
				{
					return ::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_materialized<
							line, char_type, T>(
							::std::forward<Args>(args)...);
				}
			}
		}
	}
	return ::fast_io::basic_general_concat_checked<line, char_type, T>(
		::std::forward<Args>(args)...);
}

} // namespace fast_io

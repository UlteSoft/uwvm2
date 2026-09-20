#pragma once

namespace fast_io
{

namespace details
{

/** Fixed-capacity carrier used only after every visible text code unit is compiler-known. */
template <::std::integral char_type>
inline constexpr ::std::size_t compiler_constant_text_code_unit_limit{
	(::fast_io::details::compiler_constant_materialization_max_bytes -
	 sizeof(::std::size_t)) /
	sizeof(char_type)};

template <::std::integral char_type>
struct compiler_constant_text_materialized
{
	inline static constexpr ::std::size_t capacity{
		compiler_constant_text_code_unit_limit<char_type>};
	char_type const *data{};
	::std::size_t size{};
};

/** Tests every in-range code unit without reading one which the source does not contain.
 *
 * The caller first proves that `size` itself is compiler-known.  Consequently
 * every `index >= size` guard folds before its indexed load.  A constant
 * address alone is deliberately insufficient: a pointer to mutable global
 * storage must fail unless the optimizer also proves every observed value.
 */
template <::std::integral char_type, ::std::size_t... index>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool compiler_constant_text_known_impl(
		char_type const *data, ::std::size_t size,
		::std::index_sequence<index...>) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return ((index >= size || __builtin_constant_p(data[index])) && ...);
#else
	(void)data;
	(void)size;
	return false;
#endif
}

/** Proves a bounded text spelling at the optimizer-visible source boundary. */
template <::std::integral char_type>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool compiler_constant_text_known(
		char_type const *data, ::std::size_t size) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if (!__builtin_constant_p(size) ||
		size > compiler_constant_text_code_unit_limit<char_type>)
	{
		return false;
	}
	return compiler_constant_text_known_impl(
		data, size,
		::std::make_index_sequence<
			compiler_constant_text_code_unit_limit<char_type>>{});
#else
	(void)data;
	(void)size;
	return false;
#endif
}

/** Captures one already-proved text spelling in a synchronous borrowed carrier.
 *
 * Public print, concat, and to keep every source expression alive until their
 * selected operation completes.  Retaining this pointer therefore preserves
 * the standard string/view lifetime while avoiding a fixed-capacity proxy
 * copy; the reserve emitter below is the sole character copy.
 */
template <::std::integral char_type>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr compiler_constant_text_materialized<char_type>
	compiler_constant_text_materialize(
		char_type const *data, ::std::size_t size) noexcept
{
	if (compiler_constant_text_materialized<char_type>::capacity < size)
	{
		::fast_io::fast_terminate();
	}
	return {data, size};
}

} // namespace details

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>) noexcept
{
	return ::fast_io::details::compiler_constant_text_code_unit_limit<char_type>;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>,
	char_type *output,
	::fast_io::details::compiler_constant_text_materialized<char_type> const &
		value) noexcept
{
	if (value.capacity < value.size)
	{
		::fast_io::fast_terminate();
	}
	return ::fast_io::details::non_overlapped_copy_n(
		value.data, value.size, output);
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>) noexcept
{
	// This internal proxy is an immutable pointer/length spelling emitted by a bounded non-throwing copy.
	return {};
}

/// @brief Opts standard strings into retained-scatter composition when they are range lvalue elements.
/// @details fast_io's standard-string alias is a direct `{data(), size()}` view, so the characters have exactly the
///          lifetime of the source string and advancing a stable range iterator cannot overwrite an earlier element's
///          storage. The range strategy independently requires an lvalue iterator reference; this marker therefore
///          does not admit temporary strings returned by transform views.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// A user-defined traits or allocator type contributes an associated namespace to ADL. That namespace may replace
	// the ordinary alias/forwarding protocol with scratch-backed or stateful semantics, so structural contiguity alone
	// cannot prove retained lifetime or repeatability. The standard specialization has no user-owned associated namespace.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// The default specialization aliases only its stable data pointer and size; no formatter hook is executed.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// String aliasing reads only this object's data pointer and size; it never consults a destination put cursor.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// fast_io's standard-string print vocabulary is exactly its direct data/size alias; it has no hidden element hook.
	return {};
}

/** Admits a standard string only when its complete bounded contents are optimizer-known.
 *
 * Default traits and allocation are essential: a user-associated namespace may
 * add observable alias or status semantics which a byte carrier cannot replace.
 * The character proof checks values, not merely the stable address returned by
 * `data()`, so mutable global storage remains on the ordinary scatter path.
 */
template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool
	print_compiler_constant_materialization_eligible(
		io_reserve_type_t<
			char_type,
			::std::basic_string<char_type, traits_type, allocator_type>>,
		::std::basic_string<char_type, traits_type, allocator_type> const &
			value) noexcept
{
	return ::fast_io::details::compiler_constant_text_known(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
		io_reserve_type_t<
			char_type,
			::std::basic_string<char_type, traits_type, allocator_type>>,
		::std::basic_string<char_type, traits_type, allocator_type> const &
			value) noexcept
{
	return ::fast_io::details::compiler_constant_text_materialize(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

/// @brief Records the pointer/length/content query classification for the standard string provider.
/// @details Current IO consumers classify this borrowed spelling as passive before querying it; the marker preserves a
///          complete provider proof without authorizing a redundant automatic copy.
template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type,
		::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

/// @brief Classifies the standard string's ordinary borrowed character spelling.
/// @details Default traits and allocation are the same restrictions used by the
///          alias and compiler-constant protocols above. No value query occurs;
///          print/concat consumers independently decide whether replacing this
///          source can improve their concrete destination strategy.
template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_borrowed_text_leaf(
	io_reserve_type_t<
		char_type,
		::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type,
		  typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	io_reserve_type_t<
		char_type,
		::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type strlike_buffered_print_preferred(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// The standard default-allocator string owns reusable contiguous storage and supplies amortized append/growth.
	// Custom traits or allocators remain unmarked because their associated namespaces and cost models are extensible.
	return {};
}

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__ &&                        \
	defined(_LIBCPP_VERSION) && 230000 <= _LIBCPP_VERSION &&              \
	defined(_LIBCPP_ABI_VERSION) && _LIBCPP_ABI_VERSION == 1 &&           \
	defined(_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT)
/// @brief Prefers direct fresh-result concat for one default decimal scalar on the measured standard-string target.
/// @details For the default traits and allocator, a newly constructed standard string has no externally observable
///          prefix and its maintained output adapter grows reusable SSO or heap storage.  On Apple M4 with Clang 23 and
///          libc++ 23's ABI-v1 alternate string layout, emitting one `char` scalar through that adapter avoids a
///          separate stack record and range construction while preserving the same final character sequence and
///          exception boundary.  The default adapter has no independent status, mutex, or externally visible commit
///          side effect, so its complete dispatcher is observationally equivalent to constructing from the staged
///          range.  Later Clang and libc++ versions inherit this narrowly ABI-locked policy; other standard libraries,
///          libc++ layouts, character domains, targets, traits, and allocators remain unmarked until their costs and
///          extensible hooks are independently audited.  Concat still proves the exact source shape, dispatcher
///          callability, and adapter-before-result lifetime.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<char_type, char> &&
			 ::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type
strlike_concat_fresh_fixed_scalar_direct_preferred(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}
#endif

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// On implementations where fast_io exposes the standard string's real put area, raw writes cannot relocate that
	// allocation until reserve/overflow is invoked and publishing an in-area end pointer has no independent I/O effect.
	// Restricting the proof to the standard traits/allocator also excludes user-associated ADL output hooks.
	return {};
}

/// @brief Opts a fresh default standard string into one-pass bounded concat construction.
/// @details The strategy formats into a fixed, checked local range and invokes this string's ordinary range constructor
///          only after emission succeeds. Standard traits and the default allocator are the audited cost model; custom
///          specializations retain their existing direct/precise/staging behavior.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type concat_single_pass_bounded_destination_preferred(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__ &&                        \
	defined(_LIBCPP_VERSION) && 210000 <= _LIBCPP_VERSION &&              \
	defined(_LIBCPP_ABI_VERSION) && _LIBCPP_ABI_VERSION == 1 &&           \
	defined(_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT)
/// @brief Selects ordered concat staging for long neutral-protocol packs on the measured Apple standard string.
/// @details Clang 23 with the libc++ ABI-v1 alternate layout expands repeated fresh-string append through allocation,
///          overlap, SSO, and size-publication branches for every leaf. Seven leaves repay formatting once into concat's
///          existing two-KiB inline buffer and invoking the default string's range constructor once. This marker is
///          restricted to `char`, standard traits/allocator, Apple AArch64, and the audited libc++ layout: custom traits,
///          allocators, targets, or string ABIs may have different hooks and costs. Concat independently rejects direct
///          and context formatters and sends the normalized pack through the ordinary ordered dispatcher, so an unmarked
///          scatter remains immediately consumed rather than becoming a retained descriptor.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<char_type, char> &&
			 ::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::size_t concat_ordered_staging_minimum_leaf_count(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return 7u;
}

/// @brief Extends ordered staging to retained heterogeneous neutral packs on the audited standard string.
/// @details Repeated append/growth is substantially more expensive for these packs than one ordered pass through the
///          inline staging area. The concat selector independently excludes a complete non-failing precise run when the
///          same destination can reserve its exact runtime put area and publish one cursor; that stronger one-allocation
///          construction has no staging cost to amortize. Keeping the exclusion in the consumer makes this destination
///          CPO a profitability premise only and prevents it from manufacturing source exactness or failure guarantees.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<char_type, char> &&
			 ::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
concat_ordered_staging_complete_neutral_preferred(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

/// @brief Certifies one-way ordered-staging promotion into the audited standard-string put area.
/// @details A default string owns storage disjoint from every concat source. Runtime reserve preserves its published
///          prefix, and the maintained ABI adapter exposes and publishes the same character sequence that the standard
///          range constructor would own. Promotion performs no source query and therefore may occur inside one range
///          overflow without retaining its descriptor. This opt-in also adopts direct-output exception ordering:
///          allocation failure at the promotion point may prevent later stateful producers from running, but no partial
///          result is returned. While concat caches the maintained put-area cursors, no associated output hook observes
///          the temporarily unpublished string size. If a later source throws after replacing the old terminator, the
///          audited libc++ destructor reads only the string representation and allocation ownership; it neither scans
///          nor requires that unpublished terminator, so unwinding remains valid. The enclosing Apple/Clang/libc++ ABI
///          and default traits/allocator restrictions keep custom allocation, hooks, and cursor representations outside
///          this proof.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<char_type, char> &&
			 ::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type
concat_ordered_staging_adaptive_promotion_safe(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}
#endif

/// @brief Constructs a fresh default standard string from one borrowed descriptor and a trailing line feed.
/// @details Returning a new object is the aliasing proof: its allocation cannot own the source descriptor, so one exact
///          reserve may safely precede append. A general scatter-write CPO cannot make that promise because its source
///          may point into the destination being extended. Custom traits and allocators keep their established path;
///          neither their allocation policy nor their associated ADL is covered by this opt-in.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::basic_string<char_type, traits_type, allocator_type>
strlike_construct_scatter_with_line_feed_define(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::fast_io::basic_io_scatter_t<char_type> scatter, ::std::size_t total_size)
{
	::std::basic_string<char_type, traits_type, allocator_type> result;
	result.reserve(total_size);
	if (scatter.len != 0u)
	{
		result.append(scatter.base, scatter.len);
	}
	result.push_back(::fast_io::char_literal_v<u8'\n', char_type>);
	return result;
}

#if defined(_GLIBCXX_STRING_VIEW) || defined(_LIBCPP_STRING_VIEW) || defined(_STRING_VIEW_)
/// @brief Opts standard string views into retained-scatter composition when stored as range lvalue elements.
/// @details Their alias points directly at the view's `[data(), data()+size())` range. The source-side marker is kept
///          here, beside the standard string integration, instead of inferring lifetime merely from a scatter-shaped
///          alias; that separation prevents unrelated scratch-producing aliases from receiving the same permission.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// Custom traits add an ADL namespace and may replace the view's otherwise trivial print protocol.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// The default view contributes only its stored pointer and length and has no formatter state or failure path.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// A string_view scatter is exactly its stored pointer/length pair and is independent of every output object.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// The view's complete print semantics are the characters in its stored pointer/length pair.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// Moving or destroying the standard view never changes the lifetime of its externally owned character range. This
	// stronger proof lets normalization copy the two-word view before retaining its alias. Custom traits remain excluded:
	// their associated namespace may replace alias/status forwarding with an identity-sensitive representation.
	return {};
}

/** Applies the same value-complete constant proof to a standard string view. */
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool
	print_compiler_constant_materialization_eligible(
		io_reserve_type_t<
			char_type,
			::std::basic_string_view<char_type, traits_type>>,
		::std::basic_string_view<char_type, traits_type> value) noexcept
{
	return ::fast_io::details::compiler_constant_text_known(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
		io_reserve_type_t<
			char_type,
			::std::basic_string_view<char_type, traits_type>>,
		::std::basic_string_view<char_type, traits_type> value) noexcept
{
	return ::fast_io::details::compiler_constant_text_materialize(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

/// @brief Records the pointer/length/content query classification for a standard string view.
/// @details Borrowed-text consumer gates remain responsible for keeping this graph FCO when copying cannot remove work.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

/// @brief Classifies the standard view's exact externally borrowed spelling.
/// @details The marker is independent of character constancy and lifetime
///          ownership. The existing copy-stability and synchronous-consumption
///          proofs remain responsible for any consumer which retains its range.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_borrowed_text_leaf(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}
#endif

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr auto
strlike_construct_define(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
						 char_type const *first, char_type const *last)
{
	return ::std::basic_string<char_type, traits_type, allocator_type>(first, last);
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr auto strlike_construct_single_character_define(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>, char_type ch)
{
	return ::std::basic_string<char_type, traits_type, allocator_type>(1, ch);
}

/// @brief Establishes an exact standard-string extent before exposing writable characters.
/// @details `reserve(n); data()` is intentionally insufficient: capacity does not create live characters, and portable
///          C++20 does not permit an adapter to write beyond `size()`. `resize(n)` first makes every character in the
///          requested range part of the string's observable value; the returned mutable `data()` pointer may then be
///          used by an independently proved exact-size formatter. Value initialization is the cost of this fully
///          standard fallback. Implementations with a real `buffer_strlike` protocol retain that stronger strategy and
///          are not forced through this CPO.
template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr char_type *strlike_precise_resize_and_get_begin(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str, ::std::size_t n)
{
	str.resize(n);
	return str.data();
}

/// @brief Certifies the default standard string for concat's borrowed-scatter exact-resize path.
/// @details A default-constructed standard string owns its eventual allocation; allocator requirements forbid that
///          live allocation from overlapping any separately live source string. Its resize operation observes only the
///          destination and requested extent, so borrowed descriptors remain valid until concat copies them. Custom
///          traits and allocators are excluded because their associated ADL and allocation state were not audited.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type strlike_concat_borrowed_scatter_precise_resize_safe(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

#if __cpp_lib_string_resize_and_overwrite >= 202110L
/// @brief Opts the exact default standard string into callback-owned logical-size publication.
/// @details The specialization intentionally names `std::basic_string<char_type>` with no traits or allocator template
///          parameters: that is exactly standard traits plus the default allocator. Custom traits and allocators carry
///          user-associated namespaces and independent allocation/cost behavior, neither of which was part of the
///          evidence for concat's direct-write policy. The marker supplies only destination lifetime semantics; concat
///          separately proves that its concrete source writer cannot throw after this CPO has allocated storage.
template <::std::integral char_type>
inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	io_strlike_type_t<char_type, ::std::basic_string<char_type>>) noexcept
{
	return {};
}

/// @brief Executes one exact overwrite operation on a default standard string.
/// @details `resize_and_overwrite` may allocate and therefore remains potentially throwing. Its callback receives a
///          contiguous writable extent, returns the logical size to publish, and must not retain the pointer. Forwarding
///          the exact operation category lets the standard implementation own its callback in the normal way; concat's
///          operation object is copyable, but this adapter does not impose that stronger restriction on the protocol.
template <::std::integral char_type, typename operation>
	requires requires(::std::basic_string<char_type> &str, ::std::size_t n, operation &&op) {
		str.resize_and_overwrite(n, static_cast<operation &&>(op));
	}
inline constexpr void strlike_exact_resize_and_overwrite(
	io_strlike_type_t<char_type, ::std::basic_string<char_type>>,
	::std::basic_string<char_type> &str, ::std::size_t n, operation &&op) noexcept(noexcept(str.resize_and_overwrite(n, static_cast<operation &&>(op))))
{
	str.resize_and_overwrite(n, static_cast<operation &&>(op));
}
#endif

/*
The implementation models below expose standard-string spare capacity only at
run time.  Constant evaluation cannot write beyond `size()` even when
`capacity()` is larger, so `strlike_runtime_end` deliberately collapses the
put area to the current cursor there.  The output adapter consequently uses
the ordinary append/push-back overflow protocol in constexpr evaluation while
retaining the real SSO/heap put area in generated code.  This is separate from
`buffer_strlike`: concat algorithms which write an exact result straight into
an underlying owner must first establish its logical size through the portable
precise-resize protocol.

Implementations with contiguous-container sanitizer annotations remain on the
append protocol.  Their spare capacity is deliberately poisoned until the
container publishes a larger logical size, so a stateless output cursor cannot
make that range writable before emission and then restore its annotation after
commit.
*/
/// @brief Returns the beginning of an audited standard-string put area during runtime output.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr char_type *strlike_runtime_begin(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return str.data();
}

/// @brief Returns the standard string's current logical output cursor without exposing non-live constexpr storage.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr char_type *strlike_runtime_curr(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return str.data() + str.size();
}

/// @brief Returns runtime spare-capacity end while collapsing constant evaluation to the logical cursor.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr char_type *strlike_runtime_end(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		return str.data() + str.size();
	}
	return str.data() + str.capacity();
}

/// @brief Publishes a runtime string cursor and verifies that constexpr callers did not advance beyond live storage.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
inline constexpr void strlike_runtime_set_curr(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str, char_type *pointer) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		// The constexpr window is empty, so publishing any other cursor would prove a broken output algorithm.
		if (pointer != str.data() + str.size()) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return;
	}
	::fast_io::details::string_hack::set_end_ptr(str, pointer);
	traits_type::assign(*pointer, char_type{});
}

/// @brief Reserves standard-string capacity for the runtime-only put-area adapter.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
inline constexpr void strlike_runtime_reserve(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str, ::std::size_t size)
{
	str.reserve(size);
}

/// @brief Marks the audited runtime standard-string adapter as safe for one deferred cursor commit.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr ::std::true_type strlike_runtime_deferred_obuffer_commit_safe(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

/// @brief Certifies direct exact construction in the audited default standard-string runtime put area.
/// @details A default standard string is empty; `reserve(n)` preserves that empty logical value while the implementation-
///          specific adapter exposes at least `n` writable spare characters. Publishing one cursor then establishes the
///          complete result and its terminator without an intermediate value-initialization pass. Default traits and the
///          default allocator exclude user-associated hooks and allocation models; the surrounding availability guard
///          keeps this semantic promise locked to the standard-library layouts already audited by the string hack.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr ::std::true_type strlike_concat_fresh_runtime_exact_direct_safe(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

/// @brief Certifies bounded dynamic construction in the audited default standard-string run-time put area.
/// @details `strlike_runtime_reserve` exposes writable spare capacity while the fresh result remains logically empty;
///          one final `strlike_runtime_set_curr` publishes only the prefix returned by the component writers. Default
///          traits and allocator exclude associated user hooks, and the implementation availability predicate excludes
///          poisoned or unaudited layouts. This is a destination lifetime/cursor proof only; concat separately requires
///          an explicit platform cost policy before choosing coalesced dynamic construction.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>> &&
			 ::fast_io::details::string_hack::standard_string_runtime_put_area_available)
[[nodiscard]] inline constexpr ::std::true_type strlike_concat_fresh_runtime_dynamic_direct_safe(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_append(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
			   ::std::basic_string<char_type, traits_type, allocator_type> &str, char_type const *first,
			   char_type const *last)
{
	str.append(first, last);
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_push_back(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
				  ::std::basic_string<char_type, traits_type, allocator_type> &str, char_type ch)
{
	str.push_back(ch);
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11 && defined(__linux__) && \
	defined(__x86_64__) && defined(__GLIBCXX__) && defined(_GLIBCXX_USE_CXX11_ABI) &&   \
	_GLIBCXX_USE_CXX11_ABI
/// @brief Prefers one coalesced destination for a pure dynamic-reserve concat on the audited libstdc++ ABI.
/// @details GCC 11 through 16 expand the default string's incremental print state machine once per leaf. For two or
///          more object-dependent reserve producers this repeatedly carries allocation, SSO, overlap, and cursor
///          branches through the hot concat entry. Coalescing instead measures each stable dynamic bound once and emits
///          one checked contiguous prefix: the audited run-time put area receives it directly when available, otherwise
///          concat's internal buffer feeds one ordinary range construction.
///          Default traits, allocator, C++11 ABI, Linux, and x86-64 are all part of the measured cost proof; no custom
///          allocation or associated formatter semantics inherit it. Newer GCC releases retain the highest-tested
///          strategy on the same ABI until a contrary code-generation result is established.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<char_type, char> &&
			 ::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
[[nodiscard]] inline constexpr ::std::true_type concat_dynamic_reserve_coalescing_preferred(
	io_strlike_type_t<char_type,
					  ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return {};
}
#endif

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13 && defined(__linux__) && \
	defined(__x86_64__) && defined(__GLIBCXX__) && defined(_GLIBCXX_USE_CXX11_ABI) &&   \
	_GLIBCXX_USE_CXX11_ABI
namespace details::decay
{

/// @brief Shares GCC's plain dynamic-reserve put-area state machine for the exact default `std::string` adapter.
/// @details GCC 13 through 16 on x86-64 Linux repeatedly inline this control into homogeneous concat tails, retaining
///          an invalid enclosing-object bound for `-Wstringop-overflow` and multiplying cursor branches. One out-of-line
///          chunks preserve direct writes into libstdc++'s C++11-ABI capacity, including large fragments; they add no
///          reference to the decay layer and therefore cannot change register-versus-memory parameter passing.
///          Later GCC releases inherit the highest tested policy only on the same target and ABI. Clang, other targets,
///          non-char strings, custom traits/allocators, and non-default string ABIs retain their established placement.
template <>
struct print_control_single_out_of_line_traits<
	::fast_io::io_strlike_reference_wrapper<char, ::std::string>>
{
	inline static constexpr bool value{true};
};

} // namespace details::decay
#endif

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr io_strlike_reference_wrapper<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>
io_strlike_ref(io_alias_t, ::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return {__builtin_addressof(str)};
}

template <::std::integral CharT, typename Traits = ::std::char_traits<CharT>,
		  typename Allocator = ::std::allocator<CharT>>
using basic_ostring_ref_std = io_strlike_reference_wrapper<CharT, ::std::basic_string<CharT, Traits, Allocator>>;
using ostring_ref_std = basic_ostring_ref_std<char>;
using wostring_ref_std = basic_ostring_ref_std<wchar_t>;
using u8ostring_ref_std = basic_ostring_ref_std<char8_t>;
using u16ostring_ref_std = basic_ostring_ref_std<char16_t>;
using u32ostring_ref_std = basic_ostring_ref_std<char32_t>;

template <::std::integral CharT, typename Traits = ::std::char_traits<CharT>,
		  typename Allocator = ::std::allocator<CharT>>
using basic_ostring_ref [[deprecated("Please use basic_ostring_ref_std or basic_ostring_ref_fast_io instead.")]] = ::fast_io::basic_ostring_ref_std<CharT, Traits, Allocator>;
using ostring_ref [[deprecated("Please use ostring_ref_std or ostring_ref_fast_io instead.")]] = ::fast_io::ostring_ref_std;
using wostring_ref [[deprecated("Please use wostring_ref_std or wostring_ref_fast_io instead.")]] = ::fast_io::wostring_ref_std;
using u8ostring_ref [[deprecated("Please use u8ostring_ref_std or u8ostring_ref_fast_io instead.")]] = ::fast_io::u8ostring_ref_std;
using u16ostring_ref [[deprecated("Please use u16ostring_ref_std or u16ostring_ref_fast_io instead.")]] = ::fast_io::u16ostring_ref_std;
using u32ostring_ref [[deprecated("Please use u32ostring_ref_std or u32ostring_ref_fast_io instead.")]] = ::fast_io::u32ostring_ref_std;

} // namespace fast_io

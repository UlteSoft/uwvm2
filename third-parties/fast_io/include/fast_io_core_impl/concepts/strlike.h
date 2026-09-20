#pragma once

/*
 * String-like destination protocols for IO materialization (CPO level).
 *
 * Concat and related operations use these concepts to construct, resize,
 * append to, or write directly into an arbitrary string result. The basic
 * `strlike` capability establishes result construction; stronger concepts
 * advertise buffer access, exact resize/overwrite, SSO, initialization, and
 * deferred-commit safety. These are destination capabilities, not format
 * syntax and not printable source-object protocols.
 */

namespace fast_io
{

template <::std::integral char_type, typename T>
struct io_strlike_type_t
{
	inline explicit constexpr io_strlike_type_t() noexcept = default;
};

template <::std::integral char_type, typename T>
inline constexpr io_strlike_type_t<char_type, T> io_strlike_type{};

namespace details
{

/// @brief Rejects incomplete and non-object result types before construction traits are instantiated.
template <typename T>
inline consteval bool strlike_result_object_impl() noexcept
{
	if constexpr (!::std::same_as<T, ::std::remove_cvref_t<T>> || !::std::is_object_v<T> ||
		::std::is_array_v<T> || !requires { sizeof(T); })
	{
		return false;
	}
	else
	{
		// Standard construction traits may diagnose an incomplete class. The branch above is therefore a substitution
		// gate, not merely documentation of the result object's lifetime requirement.
		return ::std::is_default_constructible_v<T>;
	}
}

template <typename T>
concept strlike_result_object = ::fast_io::details::strlike_result_object_impl<T>();

/// @brief Proves the exact writable cursor protocol consumed by the generic string output adapter.
/// @details Expression existence is insufficient here: concat subtracts and writes through all three cursors, publishes
///          the same pointer type, and treats reserve/set-current as effect-only operations. Exact results keep malformed
///          proxy cursors and count-returning mutations from becoming concept-true only to fail in an adapter body.
template <typename char_type, typename T>
concept buffer_strlike_impl = requires(T &t) {
	{ strlike_begin(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_curr(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_end(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	requires requires(char_type *ptr) {
		{ strlike_set_curr(io_strlike_type<char_type, T>, t, ptr) } -> ::std::same_as<void>;
	};
	requires requires(::std::size_t n) {
		{ strlike_reserve(io_strlike_type<char_type, T>, t, n) } -> ::std::same_as<void>;
	};
};
} // namespace details

/// @brief Proves the exact contiguous-range construction alternative of a string-like result.
/// @details Buffer and construction protocols are independent. Naming this branch lets concat test it directly instead
///          of assuming that any similarly named ADL function returns the result type required by its return statement.
template <typename char_type, typename T>
concept range_constructible_strlike =
	::std::integral<char_type> && ::fast_io::details::strlike_result_object<T> &&
	requires(char_type const *first) {
		{ strlike_construct_define(io_strlike_type<char_type, T>, first, first) } -> ::std::same_as<T>;
	};

/// @brief Proves that a scan target's ordinary token result may be range-constructed from one C-space-free fragment.
/// @details The target provider promises that, from its initial state, scanning a nonempty complete fragment containing
///          no C whitespace through EOF has exactly the same value and externally visible effects as constructing `T`
///          from that full character range. This is deliberately separate from the source marker: neither side alone
///          may bypass the context protocol, and unmarked/custom targets retain ordinary scanning.
template <typename char_type, typename T>
concept c_space_free_fragment_constructible_scan_target =
	::std::integral<char_type> &&
	::fast_io::range_constructible_strlike<char_type, ::std::remove_cvref_t<T>> &&
	requires {
		{
			scan_c_space_free_fragment_constructible(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Defines the construction/storage boundary for a string-like concat result.
/// @details `T` must be a complete, unqualified, default-constructible object and provide either exact range
///          construction or the writable cursor protocol. A cursor provider has the additional semantic obligation
///          that `begin <= curr <= end` denotes one live contiguous `char_type` array, that `set_curr` publishes only a
///          pointer in that closed range, and that `reserve(n)` preserves the existing prefix while making capacity at
///          least `n`. Any reserve operation may invalidate previously returned cursors, so consumers reacquire them.
///          These ordering, preservation, and lifetime properties are not expressible by the structural requirements.
template <typename char_type, typename T>
concept strlike =
	::std::integral<char_type> && ::fast_io::details::strlike_result_object<T> &&
	(range_constructible_strlike<char_type, T> || ::fast_io::details::buffer_strlike_impl<char_type, T>);

/// @brief Proves the exact one-character construction fast path for a string-like result.
/// @details The returned prvalue must own exactly the supplied character and be observationally equivalent to invoking
///          the range constructor with a one-element range. Exact `T` matching proves the concat return expression;
///          ownership and equivalence remain provider obligations.
template <typename char_type, typename T>
concept single_character_constructible_strlike = strlike<char_type, T> && requires(char_type ch) {
	{ strlike_construct_single_character_define(io_strlike_type<char_type, T>, ch) } -> ::std::same_as<T>;
};

/// @brief Recognition-only hook for a string-like alias customization.
/// @details No active core concat consumer currently selects this concept. A future consumer must additionally prove
///          the alias result's ownership/transport and its character-specific string protocol before retaining it;
///          expression existence alone supplies neither property.
template <typename char_type, typename T>
concept alias_strlike = requires(T &t) { strlike_alias_define(io_alias, t); };

/// @brief Refines `strlike` with the exact portable writable-cursor protocol.
/// @details Providers inherit the cursor ordering, capacity, preservation, invalidation, and lifetime obligations of
///          `strlike`. This concept intentionally does not infer that deferred cursor publication or direct scatter
///          copying is safe; those stronger properties have independent opt-in concepts below.
template <typename char_type, typename T>
concept buffer_strlike = strlike<char_type, T> && ::fast_io::details::buffer_strlike_impl<char_type, T>;

namespace details
{

/// @brief Proves a string-like put area which exists only during run-time evaluation.
/// @details Some hosted containers own writable spare capacity at run time, but the language does not permit writes
///          beyond their logical size during constant evaluation.  This protocol keeps that distinction explicit
///          instead of making the ordinary `buffer_strlike` contract conditionally false.  A constant-evaluated
///          `strlike_runtime_end` must equal `strlike_runtime_curr`; ordinary overflow/append operations then retain
///          fully standard constexpr behavior without exposing an unconstructed character range.
template <typename char_type, typename T>
concept runtime_buffer_strlike_impl = requires(T &t) {
	{ strlike_runtime_begin(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_runtime_curr(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_runtime_end(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	requires requires(char_type *ptr) {
		{ strlike_runtime_set_curr(io_strlike_type<char_type, T>, t, ptr) } -> ::std::same_as<void>;
	};
	requires requires(::std::size_t n) {
		{ strlike_runtime_reserve(io_strlike_type<char_type, T>, t, n) } -> ::std::same_as<void>;
	};
};

} // namespace details

/// @brief A hosted string-like whose spare capacity is a put area only outside constant evaluation.
/// @details The IO adapter consumes this capability exactly like an ordinary output buffer at run time.  During
///          constant evaluation the provider exposes an empty current window, so every mutation is performed by its
///          standard append/push-back protocol.  Keeping this separate from `buffer_strlike` prevents concat's raw
///          exact-placement algorithms from writing outside a standard container's live logical range.
template <typename char_type, typename T>
concept runtime_buffer_strlike = strlike<char_type, T> &&
	::fast_io::details::runtime_buffer_strlike_impl<char_type, T>;

/// @brief Unifies portable and runtime-only writable string put-area protocols for output dispatch.
template <typename char_type, typename T>
concept output_buffer_strlike = buffer_strlike<char_type, T> ||
	runtime_buffer_strlike<char_type, T>;

/// @brief Proves the public append operations used when a string-like put area overflows.
/// @details `push_back` appends exactly one character and `append(first,last)` appends the complete valid input range in
///          order. Both operations may relocate destination storage, but must preserve the existing prefix. An append
///          source may overlap the destination only when the provider's documented implementation supports it; generic
///          consumers otherwise retain the ordinary non-overlap precondition. Exact `void` results keep cursor/count
///          proxies from being silently discarded.
template <typename char_type, typename T>
concept auxiliary_strlike = strlike<char_type, T> && requires(T &t, char_type ch, char_type const *ptr) {
	{ strlike_push_back(io_strlike_type<char_type, T>, t, ch) } -> ::std::same_as<void>;
	{ strlike_append(io_strlike_type<char_type, T>, t, ptr, ptr) } -> ::std::same_as<void>;
};

/// @brief Marks an exact string-like destination which can publish storage through one overwrite callback.
/// @details This capability is intentionally separate from `precise_resize_writable_strlike`. The latter first makes
///          every character observable and may therefore initialize the whole range before a formatter overwrites it.
///          An exact-overwrite destination instead grants one callback a contiguous writable extent of at least the
///          requested size and publishes exactly the count returned by that callback. The marker alone does not prove
///          that a particular callback is accepted; `exact_resize_and_overwrite_strlike_for` checks the concrete
///          operation expression used by a strategy. Keeping both proofs destination-specific prevents an arbitrary
///          type with a similarly named member from inheriting the allocation and lifetime contract of a standard
///          string specialization.
/// @fn      strlike_exact_resize_and_overwrite_available
/// @return  std::true_type
#if __cpp_lib_string_resize_and_overwrite >= 202110L
template <typename char_type, typename T>
concept exact_resize_and_overwrite_strlike = strlike<char_type, T> && requires {
	{
		strlike_exact_resize_and_overwrite_available(io_strlike_type<char_type, T>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves the exact destination CPO for the callback object a strategy will actually pass.
/// @details The operation is tested as a named lvalue because concat constructs one operation object, then passes that
///          same object to the destination CPO. Exact `void` matching keeps count-returning or proxy adapters from
///          becoming concept-true and failing only in the strategy body. Allocation is deliberately allowed to throw;
///          the independent source concept proves that formatting after allocation cannot escape the callback.
template <typename char_type, typename T, typename operation>
concept exact_resize_and_overwrite_strlike_for =
	exact_resize_and_overwrite_strlike<char_type, T> && requires(T &str, ::std::size_t n, operation op) {
		{
			strlike_exact_resize_and_overwrite(io_strlike_type<char_type, T>, str, n, op)
		} -> ::std::same_as<void>;
	};
#else
/// @brief Keeps the callback-storage protocol unavailable until the standard library advertises its contract.
/// @details The gate belongs to the generic capability rather than only the standard-string adapter. Otherwise a
///          third-party destination defining equally named ADL hooks in a C++20 translation unit could make concat
///          instantiate an unavailable strategy and silently change both code generation and exception boundaries. The
///          always-false definitions preserve well-formed concept queries while making every admission point consume
///          the same feature-test proof. This also admits MSVC modes which expose the standardized API and macro while
///          `_MSVC_LANG` still reports its historical C++20 value.
template <typename char_type, typename T>
concept exact_resize_and_overwrite_strlike = false;

template <typename char_type, typename T, typename operation>
concept exact_resize_and_overwrite_strlike_for = false;
#endif

/// @brief Proves a type-level initial writable capacity for a buffer string-like result.
/// @details Concat uses this value in `constexpr` storage-policy branches before any result object is observed. The CPO
///          must therefore be a constant expression, not merely return `size_t`. For every default-constructed `T`, a
///          value `n` promises that the initial writable range beginning at `strlike_begin` contains at least `n`
///          characters without calling `strlike_reserve`. The value is a guaranteed lower bound, not permission to
///          write beyond the live `[begin,end)` range returned by the cursor protocol. It must fit `ptrdiff_t`, because
///          no single C++ array can provide a larger element distance.
template <typename char_type, typename T>
concept sso_buffer_strlike = buffer_strlike<char_type, T> &&
							 requires {
								 {
									 strlike_sso_size(io_strlike_type<char_type, T>)
								 } -> ::std::same_as<::std::size_t>;
								 typename ::std::integral_constant<
									 ::std::size_t,
									 strlike_sso_size(io_strlike_type<char_type, T>)>;
								 requires(strlike_sso_size(io_strlike_type<char_type, T>) <=
									 static_cast<::std::size_t>(PTRDIFF_MAX));
							 };

/// @brief Marks a string-like output adapter whose amortized-growth path is a preferred print destination.
/// @details `strlike` and `buffer_strlike` describe callable storage protocols, not their costs. In particular, a
///          user-defined adapter may flush externally, allocate on every append, or expose a deliberately small fixed
///          area. Inferring this policy from cursor syntax would make such a type enter range/context strategies which
///          were measured only for reusable storage. The exact-`true_type` CPO is therefore an explicit promise by the
///          underlying string-like implementation that synchronous incremental output normally reuses destination-
///          owned storage with amortized growth. It grants no cursor-folding permission; that stronger semantic proof
///          is represented independently below.
/// @fn      strlike_buffered_print_preferred
/// @return  std::true_type
template <typename char_type, typename T>
concept buffered_print_preferred_strlike = strlike<char_type, T> && requires {
	{
		strlike_buffered_print_preferred(io_strlike_type<char_type, T>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Selects direct construction for one fixed decimal scalar on an audited fresh result.
/// @details String-like syntax proves neither that incremental append is cheaper than stack staging plus range
///          construction nor that a default result's adapter preserves the complete print protocol.  This exact-
///          `true_type` destination marker is therefore both a cost and semantic opt-in.  For a fresh `T` and the
///          admitted single scalar, the provider promises that running the complete associated-adapter dispatcher---
///          including line, status, mutex, exception, and destructor-based commit behavior---is observationally
///          equivalent to reserve materialization followed by range construction, while normally being cheaper.
///          Concat separately proves the exact source shape, adapter callability, and adapter-before-result lifetime.
///          The marker grants no access to spare capacity and does not apply to an existing destination object.
/// @fn      strlike_concat_fresh_fixed_scalar_direct_preferred
/// @return  std::true_type
template <typename char_type, typename T>
concept concat_fresh_fixed_scalar_direct_preferred_strlike =
	strlike<char_type, T> && requires {
		{
			strlike_concat_fresh_fixed_scalar_direct_preferred(
				io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks an underlying buffer-string protocol whose put-area cursor publications may be folded.
/// @details Structural buffer conformance proves only that cursor and reserve expressions exist. It cannot prove that
///          the area stays put between raw writes, that `strlike_set_curr` has no effect beyond publishing the cursor,
///          or that output/status/locking customizations associated with `T` are observationally equivalent to direct
///          scatter copies. This explicit opt-in supplies those output-side facts to
///          `io_strlike_reference_wrapper`; the range strategy still requires independent source-side lifetime,
///          cursor-independence, and direct-print-equivalence proofs. The promise also covers exceptional destruction:
///          if a strategy has written a suffix but has not yet published its logical cursor, destroying `T` must remain
///          valid even when those raw writes replaced the previously published terminator. Keeping the marker on `T` is
///          important because a class template argument contributes its namespace to ADL: a blanket wrapper marker
///          would silently certify user-defined hooks which the wrapper itself cannot inspect or exclude.
/// @fn      strlike_deferred_obuffer_commit_safe
/// @return  std::true_type
template <typename char_type, typename T>
concept deferred_obuffer_commit_safe_strlike = buffer_strlike<char_type, T> && requires {
	{
		strlike_deferred_obuffer_commit_safe(io_strlike_type<char_type, T>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves that a string-like result can establish one exact writable logical extent.
/// @details This capability is deliberately independent of `buffer_strlike`. The CPO must first make the destination's
///          observable size exactly `n`, then return the beginning of one contiguous array containing at least `n`
///          live mutable `char_type` objects. `[result, result + n)` remains valid until a later non-const operation or
///          destruction. No character beyond that logical range is exposed and no cursor mutation is implied. The CPO
///          may throw; returning proves only destination storage and lifetime, while the source's precise-print protocol
///          separately proves the required extent. This separation permits portable `std::basic_string::resize` use
///          without treating spare capacity as constructed character storage.
/// @fn      strlike_precise_resize_and_get_begin
/// @param   io_strlike_type<char_type, T> exact destination tag
/// @param   T&                            destination being constructed
/// @param   std::size_t                   exact final character count
/// @return  char_type*                    beginning of the live writable logical range
template <typename char_type, typename T>
concept precise_resize_writable_strlike = strlike<char_type, T> && requires(T &str, ::std::size_t n) {
	{
		strlike_precise_resize_and_get_begin(io_strlike_type<char_type, T>, str, n)
	} -> ::std::same_as<char_type *>;
};

/// @brief Opts a precise-resize result into concat's borrowed-scatter exact-copy strategy.
/// @details Structural resize capability does not prove independence from source descriptors: a user string-like CPO
///          may consult external state, reuse externally supplied storage, or mutate a borrowed source while resizing.
///          This exact-`true_type` marker promises that default construction followed by precise resize produces result-
///          owned storage disjoint from every live input scatter, and that the resize operation neither reads nor
///          mutates those scatter descriptors or their character ranges. Concat still separately proves the concrete
///          resize expression and every source extent. Keeping this destination opt-in explicit prevents a custom
///          string-like type from inheriting standard-string alias and lifetime semantics from matching syntax alone.
/// @fn      strlike_concat_borrowed_scatter_precise_resize_safe
/// @return  std::true_type
template <typename char_type, typename T>
concept concat_borrowed_scatter_precise_resize_safe_strlike =
	precise_resize_writable_strlike<char_type, T> && requires {
		{
			strlike_concat_borrowed_scatter_precise_resize_safe(
				io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Runtime-put-area counterpart of `deferred_obuffer_commit_safe_strlike`.
/// @details The promise covers only the non-constant writable window advertised by `runtime_buffer_strlike`; its
///          constexpr window is empty and therefore never receives a deferred raw copy.
template <typename char_type, typename T>
concept runtime_deferred_obuffer_commit_safe_strlike =
	runtime_buffer_strlike<char_type, T> && requires {
		{
			strlike_runtime_deferred_obuffer_commit_safe(io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Certifies a fresh result for one exact reserve followed by one runtime cursor publication.
/// @details Runtime put-area shape and deferred-commit safety do not prove that a default result is logically empty or
///          that writing its complete requested range is equivalent to fresh concat construction. This explicit marker
///          supplies those destination semantics: default construction establishes `begin == curr`, reserving the exact
///          final extent preserves that empty prefix and exposes a writable range of at least that extent, and one final
///          runtime cursor publication makes exactly the written prefix observable. Source relocation, exception, and
///          exact-writer proofs remain independent requirements of the consuming concat strategy.
/// @fn      strlike_concat_fresh_runtime_exact_direct_safe
/// @return  std::true_type
template <typename char_type, typename T>
concept concat_fresh_runtime_exact_direct_strlike =
	runtime_deferred_obuffer_commit_safe_strlike<char_type, T> && requires {
		{
			strlike_concat_fresh_runtime_exact_direct_safe(io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Proves that exact resize creates writable characters without first initializing the overwritten range.
/// @details `precise_resize_writable_strlike` is a lifetime/capability contract and intentionally permits portable
///          `std::basic_string::resize`, which value-initializes every new character. This refinement is only a cost
///          proof. Returning true states that establishing logical size `n` does not perform a character-writing pass
///          over `[begin, begin + n)` before the exact formatter writes that complete range. Ordinary concat uses the
///          proof for initialization-sensitive leaves; semantic concat retains its independent whole-graph policy.
///          The marker grants no permission to write outside the live logical range established by the resize CPO.
/// @fn      strlike_precise_resize_without_initialization
/// @return  std::true_type
template <typename char_type, typename T>
concept precise_resize_without_initialization_strlike =
	precise_resize_writable_strlike<char_type, T> && requires {
		{
			strlike_precise_resize_without_initialization(io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

} // namespace fast_io

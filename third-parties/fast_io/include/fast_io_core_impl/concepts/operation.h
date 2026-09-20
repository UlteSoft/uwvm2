#pragma once

/*
 * Value representation and semantic policy protocols (CPO level).
 *
 * This file is the open object side of fast_io. Its major families are:
 *
 * - scan leaves: precise-reserve, contiguous, terminal-padding, iterative, and
 *   context scanners;
 * - print leaves: static/dynamic/precise reserve, scatter and reserve-scatter,
 *   context, direct-output, staged, and cached representations;
 * - source normalization: `io_print_alias`/`io_scan_alias` followed by
 *   character-domain forwarding;
 * - semantic objects: `io_null`, `parameter`, and the protocol support consumed
 *   by pack/condition/width nodes;
 * - proof and cost markers: borrowing, repeatability, deferred commit,
 *   compiler-constant safety, ABI transport, coalescing, and stream thresholds.
 *
 * These concepts are deliberately not one flat "printable" hierarchy. A leaf
 * capability says how an object can be materialized; a safety/equivalence
 * marker says when a strategy may reuse or defer that representation; a cost
 * marker chooses among already-correct strategies. The C++ expressions prove
 * shape only, while provider comments define lifetime, bounds, state, and
 * observational obligations. IO-level print/scan/concat code composes the
 * capabilities into a complete operation.
 */

namespace fast_io
{

/// @brief Reports the readable padding owned by a contiguous range.
/// @details This is a run-time capability query, not a claim that every object of the provider type currently owns
///          padding. For a particular range `r`, let `B = ranges::data(r)`, `E = ranges::end(r)`, and
///          `N = ranges::size(r)`. For `N > 0`, `E == B + N`; for `N == 0`, `B == E` is sufficient and neither the
///          provider nor consumer needs to perform arithmetic on a possibly null equal pair. If
///          `P = contiguous_range_padding_size(r)`, the provider promises the following model:
///
///          * `P == 0` supplies no permission beyond `[B,E)`.
///          * `P > 0` proves that `E` belongs to a storage range in which `E + P` is defined and `[E,E+P)` consists
///            of live, contiguous, readable range-value objects owned by the same range lifetime. `P` is measured in
///            range elements, not bytes.
///          * `[B,E)` remains the complete semantic range. Padding does not change `size()`, `end()`, EOF, or the
///            maximum cursor that a consumer may publish.
///          * The query has no side effects and its result remains valid until an operation which may invalidate the
///            range's data pointer, size, capacity, mapping, or ownership.
///
///          The padding values need not be initialized to a sentinel unless a stronger provider contract says so.
///          Consequently a consumer may use them only for memory-safe speculative/vector loads whose semantic result
///          is masked to `[B,E)`; changing bytes solely in `[E,E+P)` must not change parsing results or target effects.
///
///          Formally, compose this promise with `terminal_contiguous_padding_scannable` using the same `(B,E,P)`.
///          Every scanner read is then in `[B,E+P)`, hence inside provider-owned live storage; every returned cursor is
///          in `[B,E]`, hence outside the padding; and padding noninterference preserves the result of scanning the
///          semantic sequence `[B,E)`. These three facts prove memory safety, unchanged logical EOF, and observational
///          equivalence to a bounds-respecting scan. The C++ concept can check only the exact, non-throwing query
///          expression; the range/storage/lifetime equations above are semantic obligations of its provider.
/// @fn       contiguous_range_padding_size
/// @param    T const&                                     the contiguous range
/// @return   ::std::size_t                                readable padding in range elements; zero means none
template <typename T>
concept contiguous_range_with_padding = requires(::std::remove_cvref_t<T> const &range) {
	{
		contiguous_range_padding_size(range)
	} noexcept -> ::std::same_as<::std::size_t>;
	// Padding participates in speculative reads, so neither an unwind nor a deterministic error may escape its query.
	requires FAST_IO_HERBCEPTIONS_NOTHROWS(contiguous_range_padding_size(range));
};

/// @brief    contiguous_scannable
/// @details  That a type is contiguous scannable
///           is that it can be scanned from a contiguous memory region
///           passed in as a pointer to the beginning and end of the region
///           as the 2nd and 3rd arguments in `scan_contiguous_define`.
/// @fn       scan_contiguous_define
/// @brief    Scans an object from a contiguous memory region.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, T>    tag-invoke
/// @param    char_type const*                              a pointer to the beginning of the buffer
/// @param    char_type const*                              a pointer to the end of the buffer
/// @param    T                                             the object to be scanned, can be any passing style
/// @return   ::fast_io::parse_result<char_type const*>     a pointer to the next after scanning,
///                                                         and a parse code indicating parsing state
template <typename char_type, typename T>
concept contiguous_scannable = ::std::integral<char_type> && requires(char_type const *begin, char_type const *end, T &t) {
	{
		scan_contiguous_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, begin, end, t)
	} -> ::std::same_as<parse_result<char_type const *>>;
};

/// @brief Proves that an ordinary contiguous scanner always returns a cursor in its supplied closed range.
/// @details The base contiguous protocol exposes an open ADL customization point, so a dispatcher cannot infer pointer
///          provenance merely from the return type.  Without this marker it validates the returned address and element
///          alignment before publishing a cursor.  A provider returning `true_type` promises that every result of
///          `scan_contiguous_define(tag, first, last, target)`, for every parse code and target effect, is either `first`,
///          `last`, or a pointer to a live `char_type` element between them in the same array.  That promise makes direct
///          pointer commit valid and removes the generic integer-address validation from trusted leaf scanners.
///
///          This is a proof marker, not a preference or cost hint.  It does not cover context or padding CPOs, does not
///          authorize reads outside `[first,last)`, and does not weaken any parse-code rule.  Third-party scanners remain
///          checked unless they explicitly accept the complete semantic obligation.
/// @fn       scan_contiguous_result_in_range
/// @return   ::std::true_type
template <typename char_type, typename T>
concept contiguous_scanner_result_in_range =
	contiguous_scannable<char_type, T> && requires {
		{
			scan_contiguous_result_in_range(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Proves that one normalized printable fragment never emits a C whitespace character.
/// @details This source-side marker allows an in-memory conversion to distinguish a complete lexical token from an
///          arbitrary printable range. The promise covers every value of the advertised printable type and the exact
///          character domain named by the tag. It does not imply a static size, replayability, or absence of other
///          punctuation. Providers opt in by returning `true_type` from `print_fragment_c_space_free(tag)`.
template <typename char_type, typename T>
concept c_space_free_print_fragment =
	::std::integral<char_type> && requires {
		{
			print_fragment_c_space_free(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief A terminal whole-range scanner with a separate padding-aware CPO.
/// @details `scan_contiguous_padding_define(tag, first, last, padding, target)` receives two distinct boundaries.
///          `last` is the true terminal position of the input, while `padding` is the number of additional readable
///          `char_type` objects immediately following it. The distinct CPO name is intentional: adding a padding
///          parameter to the ordinary `scan_contiguous_define` overload set can alter the ABI, inlining, and register
///          allocation of scanners which never consume padding. A type opts into this protocol only by defining the
///          separately recognized terminal-padding operation.
///
///          Formal contract. Let `p = padding` and `S = [first,last)`. When `first != last`, let
///          `n = last - first`; when they are equal, let `n = 0` without requiring pointer subtraction. If `p == 0`,
///          let the readable domain `A = S` without forming `last + 0`; if `p > 0`, let
///          `A = [first,last+p)`. The caller proves that all nontrivial pointer operations above are within one live
///          storage range, that `n+p` is representable in both `size_t` and `ptrdiff_t`, and that every `char_type`
///          object in `A` is readable for the call. The scanner:
///
///          1. may read only addresses in `A` and may not write through or retain either input pointer;
///          2. must treat only `S` as input, so its returned iterator is in the closed interval `[first,last]`;
///          3. must not consume padding: success, failure, consumed iterator, target effects, and thrown exceptions are
///             invariant under replacing the values in `[last,last+p)` while keeping `S` fixed;
///          4. has ordinary terminal whole-range semantics at `last`; padding is never an extra refill and cannot turn
///             an incomplete token at the real EOF into a longer token.
///
///          For verification, write `R` for the set of addresses read and `i` for the returned iterator. Premise (1)
///          gives `R subseteq A`, while the provider of the matching range-padding query proves `A` readable, hence no
///          read is out of bounds. Premise (2) gives `first <= i <= last`, so committing `i` preserves the real file
///          cursor and cannot enter `[last,last+p)`. Premise (3) makes the observable result a function of `S` alone.
///          Therefore exposing padding changes only the implementation's legal load width, never the abstract parse.
///          These are semantic requirements; the concept can structurally prove only the exact call and result type.
///
///          The dispatcher invokes this CPO only for a terminal input. A scanner which also provides the ordinary
///          contiguous CPO reaches this operation only when the available padding is positive; a padding-only scanner
///          may receive zero so its historical five-argument protocol remains usable without inventing a four-argument
///          fallback. Implementations must preserve ordinary terminal semantics when their specialized tail leaf is not
///          applicable; eligibility failure may not modify the target.
/// @fn       scan_contiguous_padding_define
/// @param    ::fast_io::io_reserve_type_t<char_type, T>    tag-invoke
/// @param    char_type const*                              beginning of the semantic input
/// @param    char_type const*                              true one-past end of the semantic input
/// @param    ::std::size_t                                 readable elements after the true end
/// @param    T                                             the object to be scanned, can be any passing style
/// @return   ::fast_io::parse_result<char_type const*>     next semantic cursor and parse status
template <typename char_type, typename T>
concept contiguous_padding_scannable_define =
	::std::integral<char_type> &&
	requires(char_type const *begin, char_type const *end, ::std::size_t padding, T &t) {
		{
			scan_contiguous_padding_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, begin, end, padding, t)
		} -> ::std::same_as<parse_result<char_type const *>>;
	};

namespace details
{

/*
Recognize the historical padding position without accepting an unrelated
five-argument overload merely because size_t converts to its mode type. The
probe has the one intended conversion. Its hidden deleted bool peer makes a
bool/int/etc. provider ambiguous, while an exact size_t provider wins by the
better trailing standard-conversion sequence. This also admits an exact
size_t overload when an unrelated mode overload coexists with it.
*/
template <::std::integral char_type, typename target_type>
struct scan_contiguous_padding_size_probe
{
	inline constexpr operator ::std::size_t() const noexcept
	{
		return {};
	}

	friend parse_result<char_type const *> scan_contiguous_define(
		io_reserve_type_t<char_type, ::std::remove_cvref_t<target_type>>,
		char_type const *, char_type const *, bool, target_type &) = delete;
};

} // namespace details

template <typename char_type, typename T>
concept contiguous_padding_scannable_legacy =
	::std::integral<char_type> &&
	requires(char_type const *begin, char_type const *end, ::std::size_t padding,
			 ::fast_io::details::scan_contiguous_padding_size_probe<
				 char_type, ::std::remove_reference_t<T>> exact_padding,
			 T &t) {
		{
			// Preserve the historical overload spelling.  It is recognized for
			// source compatibility, while new providers should use the distinct
			// padding CPO above so ordinary scanner lookup remains unchanged.
			scan_contiguous_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, begin, end, padding, t)
		} -> ::std::same_as<parse_result<char_type const *>>;
		{
			// The second call is unevaluated and filters implicit size_t-to-mode
			// conversions which would otherwise silently opt an unrelated overload in.
			scan_contiguous_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, begin, end,
				exact_padding, t)
		} -> ::std::same_as<parse_result<char_type const *>>;
	};

template <typename char_type, typename T>
concept contiguous_padding_scannable_protocol =
	contiguous_padding_scannable_define<char_type, T> ||
	contiguous_padding_scannable_legacy<char_type, T>;

template <typename char_type, typename T>
concept terminal_contiguous_padding_scannable =
	contiguous_padding_scannable_protocol<char_type, T>;

/// A padding protocol can be declared independently of the ordinary
/// contiguous scanner.  This recognition-only refinement is useful to
/// capability tests and legacy providers; terminal dispatch may use this
/// protocol even when no four-argument ordinary scanner is provided.
template <typename char_type, typename T>
concept contiguous_scannable_with_padding =
	contiguous_padding_scannable_protocol<char_type, T>;

namespace details
{

/// @brief Validates that a scanner result has the address and alignment of the contiguous range supplied to its CPO.
/// @details This predicate lives with protocol recognition, rather than either stream or terminal-range dispatch, so
///          both consumers apply one closed-interval rule despite their include order. At run time an integer-address
///          check cannot prove C++ pointer provenance; callers that need an offset must therefore use the validated
///          address difference and apply it to their original cursor instead of subtracting the untrusted result.
template <::std::integral char_type>
inline constexpr bool scan_iterator_in_current_chunk(
	char_type const *first, char_type const *last, char_type const *result) noexcept
{
	if (::std::is_constant_evaluated())
	{
		// A conforming constant-evaluated scanner can only form a pointer into the supplied array (or one-past it), so
		// language pointer ordering is both available and sufficient in this branch.
		return first <= result && result <= last;
	}
	auto const first_address{reinterpret_cast<::std::uintptr_t>(first)};
	auto const last_address{reinterpret_cast<::std::uintptr_t>(last)};
	auto const result_address{reinterpret_cast<::std::uintptr_t>(result)};
	// Relational comparison of pointers to unrelated objects has only an unspecified ordering. Address comparison makes
	// a malformed ADL result deterministically rejectable before subtraction, while the modulus proves that a wide-
	// character cursor still designates an element boundary within the closed interval.
	return first_address <= result_address && result_address <= last_address &&
		   ((result_address - first_address) % sizeof(char_type) == 0u);
}

/// @brief Validates the object/lifetime requirements shared by incremental protocol states.
/// @details A state owner constructs exactly one mutable object. Encoding that rule before protocol-specific aliases
///          prevents requires-expression parameter adjustment from admitting arrays and prevents a cv-qualified state
///          from being recognized even though it cannot satisfy the mutable ownership contract.
template <typename state_type>
inline consteval bool context_state_object_impl() noexcept
{
	using value_type = ::std::remove_cvref_t<state_type>;
	if constexpr (!::std::same_as<state_type, value_type> || !::std::is_object_v<state_type> ||
				  ::std::is_array_v<state_type> || !requires { sizeof(value_type); })
	{
		// Standard construction concepts are permitted to diagnose an incomplete class in their underlying traits.
		// Reject it before those traits are formed so an advertised forward declaration remains an ordinary false CPO.
		return false;
	}
	else
	{
		return ::std::default_initializable<state_type>;
	}
}

template <typename state_type>
concept context_state_object = ::fast_io::details::context_state_object_impl<state_type>();

/// @brief Extracts the state advertised by a context-scanner customization.
/// @details Keeping this dependent expression in one alias makes recognition, dispatch, and storage policy inspect the
///          same type and avoids repeating an expensive ADL probe throughout every constraint.
template <typename char_type, typename T>
using scan_context_state_t = typename ::std::remove_cvref_t<decltype(scan_context_type(io_reserve_type<char_type, ::std::remove_cvref_t<T>>))>::type;

/// @brief Validates the object/lifetime requirements of an advertised context state.
/// @details The dynamic owner constructs exactly one mutable object. Encoding that rule once prevents requires-
///          expression parameter adjustment from accidentally admitting arrays and gives storage helpers the same
///          cv/default-construction contract as protocol recognition.
template <typename state_type>
concept scan_context_state_object = ::fast_io::details::context_state_object<state_type>;

/// @brief Extracts the state advertised by a context-printer customization.
/// @details Recognition and every active print dispatcher use this normalized alias, so ADL is performed with the
///          same unqualified printable tag and storage cannot silently select a different state type.
template <typename char_type, typename T>
using print_context_state_t = typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, ::std::remove_cvref_t<T>>))>::type;

/// @brief Applies the common incremental-state object contract to context printing.
template <typename state_type>
concept print_context_state_object = ::fast_io::details::context_state_object<state_type>;

} // namespace details

/// @brief    context_scannable
/// @details  That a type is context scannable
///           is that it can be scanned multiple times until it's fully scanned
///           with the scanning state stored in a context object,
///           each time from a contiguous memory region.
/// @note     A scanner for refillable input MUST provide this protocol when a token can cross a buffer boundary.
///           A contiguous-only scanner remains valid for a terminal ibuffer whose complete remaining span is known.
/// @fn       scan_context_type
/// @brief    Indicates the type of your context object for context scannable types.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, T>    tag-invoke
/// @return   ::fast_io::io_type_t<your_context_type>       the type of the context object
/// @struct   your_context_type
/// @brief    The context object, usually stores partial results.
/// @fn       scan_context_define
/// @brief    Partially scans an object from a contiguous memory region with a context object.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, T>    tag-invoke
/// @param    your_context_type&                            the context object
/// @param    char_type const*                              a pointer to the beginning of the buffer
/// @param    char_type const*                              a pointer to the end of the buffer
/// @param    T                                             the object to be scanned, can be any passing style
/// @return   ::fast_io::parse_result<char_type const*>     a pointer to the next after scanning,
///                                                         and a parse code indicating parsing state
/// @note     It's not recommended to write to T if the scanning is still partial, so as to hold strong exception guarantee.
/// @fn       scan_context_eof_define
/// @brief    Indicates that the scanning meets an EOF.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, T>    tag-invoke
/// @param    your_context_type&                            the context object
/// @param    T                                             the object to be scanned, can be any passing style
/// @return   ::fast_io::parse_code                         a parse code indicating parsing state
/// @note     If EOF makes a partial prefix invalid, a context scanner may have
///           already consumed that prefix. Scanners that can rewind within the
///           current buffer may additionally provide scan_context_eof_rewind_size.
/// @note     The advertised state must be an unqualified, non-array object type. A const/reference state cannot
///           satisfy the mutable cross-refill ownership contract, while a requires-expression silently adjusts an
///           array parameter to a pointer and would recognize a protocol that cannot be constructed as one state.
template <typename char_type, typename T>
concept context_scannable = ::std::integral<char_type> && requires(char_type const *begin, char_type const *end, T &t) {
	requires requires(::fast_io::details::scan_context_state_t<char_type, T> &st) {
		requires ::fast_io::details::scan_context_state_object<
			::fast_io::details::scan_context_state_t<char_type, T>>;
		{
			scan_context_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, st, begin, end, t)
		} -> ::std::same_as<parse_result<char_type const *>>;
		{
			scan_context_eof_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, st, t)
		} -> ::std::same_as<parse_code>;
	};
};

/// @brief Proves that every ordinary context-scanner transition returns a cursor in its supplied closed range.
/// @details Context scanning is an open ADL protocol just like contiguous scanning. Generic dispatch therefore checks
///          the numeric address and element alignment of every returned iterator before publishing it. A provider may
///          remove that repeated validation by returning `true_type` from this marker and accepting the complete proof
///          obligation for every state, parse code, target effect, and input span passed to `scan_context_define`.
///
///          This marker covers only the ordinary stateful CPO. It does not prove the separate transactional current-
///          chunk accelerator, the EOF transition, or a contiguous scanner. Third-party context scanners remain
///          checked unless they explicitly advertise this contract.
/// @fn       scan_context_result_in_range
/// @return   ::std::true_type
template <typename char_type, typename T>
concept context_scanner_result_in_range =
	context_scannable<char_type, T> && requires {
		{
			scan_context_result_in_range(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Optional transactional fast path for a context scanner on one buffered input chunk.
/// @details `scan_context_current_chunk_minimum_size(tag)` advertises the smallest buffer capacity for which dispatching
///          this accelerator is useful. The dispatcher enables it only when the input has a compile-time minimum-window
///          protocol at least that large; context-oriented small buffers retain their original code shape.
///
///          `scan_context_current_chunk_define(tag, first, last, target)` may complete a value only when the supplied
///          chunk contains enough input to decide the result without relying on `last` as semantic EOF. A miss must
///          return exactly `{first, parse_code::partial}` and must not modify `target`; the dispatcher then invokes the
///          ordinary context state machine from the unchanged cursor. Any decisive result must return an iterator in
///          `[first,last)`, because `last` is treated as a chunk boundary even for a terminal input. This separate CPO
///          avoids the former unsafe optimization of invoking a terminal contiguous scanner speculatively and then
///          attempting to repair its target effects when the token reached `last`.
///
///          The operation is an accelerator only. Context-only scanners and hybrid scanners that do not provide the
///          transactional CPO retain the identical context dispatch and code shape. Structural recognition can prove
///          only the call and result type; the no-effect-on-partial rule is a semantic requirement on the customization.
template <typename char_type, typename T>
concept current_chunk_context_scannable =
	context_scannable<char_type, T> &&
	requires {
		typename ::std::integral_constant<::std::size_t,
										  scan_context_current_chunk_minimum_size(
											  io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scan_context_current_chunk_minimum_size(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		requires(scan_context_current_chunk_minimum_size(
					 io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u);
		requires(scan_context_current_chunk_minimum_size(
					 io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <
				 static_cast<::std::size_t>(PTRDIFF_MAX));
	} &&
	requires(char_type const *begin, char_type const *end, T &t) {
		{
			scan_context_current_chunk_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, begin, end, t)
		} -> ::std::same_as<parse_result<char_type const *>>;
	};

/// @brief Allows a current-chunk accelerator to remove generic cursor validation from its hot path.
/// @details The provider promises that every decisive result from `scan_context_current_chunk_define` belongs to the
///          supplied half-open range. Partial misses remain governed by the stronger transactional contract above.
template <typename char_type, typename T>
concept current_chunk_context_scanner_result_in_range =
	current_chunk_context_scannable<char_type, T> && requires {
		{
			scan_context_current_chunk_result_in_range(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Opts a hybrid scanner into terminal contiguous dispatch.
/// @details Merely providing both contiguous and context CPOs proves two syntactic capabilities, not that selecting
///          either one is observationally interchangeable. This marker is the scanner author's proof that, when the
///          supplied span is the complete remaining input, `scan_contiguous_define` has the same target effects,
///          parse result, and consumed cursor as running the context protocol through EOF. The promise includes an
///          empty span. It says nothing about refillable chunks; those always retain the context state machine.
/// @fn       scan_context_terminal_contiguous_equivalent
/// @return   ::std::true_type
template <typename char_type, typename T>
concept terminal_contiguous_context_scannable =
	contiguous_scannable<char_type, T> && context_scannable<char_type, T> && requires {
		{
			scan_context_terminal_contiguous_equivalent(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Publishes a target-side cost preference for small complete-input staging in `to`.
/// @details Terminal contiguous/context equivalence is the legality proof; this independent marker says only that, for
///          a small complete character run, one contiguous scan is normally preferable to repeated context transitions.
///          It grants no permission to materialize observable sources, allocate dynamic storage, or assume a target-
///          derived input bound. The `to` relation planner combines this preference with source purity and boundedness.
/// @fn       to_terminal_contiguous_staging_preferred
/// @return   ::std::true_type
template <typename char_type, typename T>
concept to_terminal_contiguous_staging_preferred_target =
	::std::integral<char_type> && requires {
		{
			to_terminal_contiguous_staging_preferred(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Opts a hybrid scanner into terminal padded-contiguous dispatch.
/// @details This promise is intentionally distinct from
///          `scan_context_terminal_contiguous_equivalent`. A scanner may use
///          its padded CPO only when the input itself exposes terminal padding,
///          without changing the ordinary context/contiguous selection or code
///          generation for unpadded inputs.
/// @fn       scan_context_terminal_padding_equivalent
/// @return   ::std::true_type
template <typename char_type, typename T>
concept terminal_padding_context_scannable =
	terminal_contiguous_padding_scannable<char_type, T> &&
	context_scannable<char_type, T> && requires {
		{
			scan_context_terminal_padding_equivalent(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief    reserve_printable
/// @details  A reserve-printable type can be materialized into caller-owned contiguous storage whose capacity is
///           known from the type. `print_reserve_size` is an upper bound, not necessarily the number of characters
///           emitted; the pointer returned by `print_reserve_define` identifies the actual end. Code that needs an
///           exact allocation must use one of the precise-reserve protocols below.
/// @fn       print_reserve_size
/// @brief    Returns a compile-time-constant upper bound on the required buffer capacity.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @return   ::std::size_t                                 the size of the buffer to be reserved
/// @fn       print_reserve_define
/// @brief    Prints the object to a buffer.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param    char_type*                                    a pointer to the beginning of the buffer
/// @param    T                                             the object to be printed
/// @return   char_type*                                    a pointer to the next after printing
template <typename char_type, typename T>
concept reserve_printable = ::std::integral<char_type> && requires(T t, char_type *ptr) {
	{
		print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::size_t>;
	typename ::std::integral_constant<
		::std::size_t,
		print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
	// The dispatcher uses this value as a constant array/capacity and performs
	// pointer differences in `ptrdiff_t`.  A syntactically callable run-time CPO,
	// zero bound, or unrepresentable bound would make this concept true only to
	// fail much deeper in the selected strategy.  Forming `integral_constant` is
	// the SFINAE-friendly constant-expression proof; the following value checks
	// are reached only after that proof, so a run-time CPO makes the concept false
	// instead of producing a hard diagnostic inside a nested requirement.
	requires(print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u);
	requires(print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <
			 static_cast<::std::size_t>(PTRDIFF_MAX));
	{
		print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t)
	} -> ::std::same_as<char_type *>;
};

/// @brief    dynamic_reserve_printable
/// @details  A dynamic-reserve-printable type reports an object-dependent contiguous capacity before it is
///           materialized. As with `reserve_printable`, the reported size is an upper bound and define returns the
///           actual end pointer. Measuring may inspect the object, so a composite that measures and then defines
///           must provide a stable multi-pass source.
/// @fn       print_reserve_size
/// @brief    Returns an object-dependent upper bound on the required buffer capacity.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param    T                                             the object to be printed
/// @return   ::std::size_t                                 the size of the buffer to be reserved
/// @fn       print_reserve_define
/// @brief    Prints the object to a buffer.
/// @tparam   <auto-inferred>
/// @param    ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param    char_type*                                    a pointer to the beginning of the buffer
/// @param    T                                             the object to be printed
/// @return   char_type*                                    a pointer to the next after printing
template <typename char_type, typename T>
concept dynamic_reserve_printable = ::std::integral<char_type> && requires(T t, char_type *ptr) {
	{
		print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
	} -> ::std::same_as<::std::size_t>;
	{
		print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t)
	} -> ::std::same_as<char_type *>;
};

namespace details::single_pass_bounded_materialization_adl
{

// The dependent tag arguments defer these unqualified names to ADL at template
// instantiation. Do not add deleted poison pills here: GCC 11 fixes that deleted
// overload set too early and then cannot discover formatter CPOs declared by
// later headers. The public accessor has a distinct `_invoke` name, so leaving
// these names unbound cannot recurse through ordinary lookup.

/// @brief Recognizes the destination-neutral spelling of the bounded one-pass source protocol.
/// @details Marker and size are deliberately validated as one indivisible protocol, so a partial customization can
///          never select a materialization strategy whose complete contract the source did not provide.
template <typename char_type, typename T>
concept protocol = ::std::integral<char_type> && requires(
	::std::remove_cvref_t<T> const &value, ::std::size_t maximum_size) {
	{
		single_pass_bounded_materialization_preferred(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
	{
		single_pass_bounded_materialization_size(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
			value, maximum_size)
	} noexcept -> ::std::same_as<::std::size_t>;
	// A failure would be indistinguishable from the SIZE_MAX sentinel consumed by both print and concat.
	requires FAST_IO_HERBCEPTIONS_NOTHROWS(single_pass_bounded_materialization_size(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value, maximum_size));
};

template <typename char_type, typename T>
	requires protocol<char_type, T>
[[nodiscard]] inline constexpr ::std::size_t size(
	T const &value, ::std::size_t maximum_size) noexcept
{
	return single_pass_bounded_materialization_size(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
		value, maximum_size);
}

} // namespace details::single_pass_bounded_materialization_adl

/// @brief Marks a source with a cheap, non-fatal bound suitable for one-pass contiguous materialization.
/// @details This is a destination-neutral source capability. The selected size CPO observes a cvref-normalized const
///          source and must return SIZE_MAX when the representation cannot fit `maximum_size`; it must not consume or
///          modify the source, terminate, allocate destination storage, or infer a print/concat policy. Emission follows
///          as a separate operation. The exact `true_type`, `size_t`, and noexcept requirements make the protocol usable
///          by both print and concat. This development API intentionally has one spelling; retaining an older concat-
///          specific alias would let the two consumers silently recognize different source contracts.
template <typename char_type, typename T>
concept single_pass_bounded_materialization_source =
	::fast_io::details::single_pass_bounded_materialization_adl::protocol<
		char_type, T>;

/// @brief Allows `to` to format a normalized source even when the scanner may not consume its characters.
/// @details The exact-true marker promises that fully formatting the unchanged object once and discarding the result is
///          observationally equivalent to not formatting it: no visible side effect, state consumption, additional
///          exception, or unstable spelling may result. This is deliberately stronger than borrowed/repeatable scatter
///          provenance and deliberately independent of every size protocol. Unmarked user sources retain lazy suffix
///          formatting even when their representation otherwise looks cheap or bounded.
/// @fn       print_eager_materialization_safe
/// @return   ::std::true_type
template <typename char_type, typename T>
concept eager_materialization_safe_printable =
	::std::integral<char_type> && requires {
		{
			print_eager_materialization_safe(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

template <::std::integral char_type, ::std::integral source_char_type>
	requires(::std::same_as<char_type, ::std::remove_cv_t<source_char_type>>)
inline constexpr ::std::true_type
print_eager_materialization_safe(io_reserve_type_t<char_type, source_char_type>) noexcept
{
	// A same-domain character leaf is copied as one code unit and has no formatter state or failure path.
	return {};
}

/// @brief Calls the size member of the selected destination-neutral source protocol.
/// @details Protocol selection occurs once for the complete marker/size pair. This generic accessor is the only
///          operation used by print and concat algorithms, preventing either layer from depending on the other's CPO
///          name or header order.
template <typename char_type, typename T>
	requires ::fast_io::single_pass_bounded_materialization_source<char_type, T>
[[nodiscard]] inline constexpr ::std::size_t
single_pass_bounded_materialization_size_invoke(
	T const &value, ::std::size_t maximum_size) noexcept
{
	return ::fast_io::details::single_pass_bounded_materialization_adl::size<
		char_type>(value, maximum_size);
}

namespace details
{

/// @brief Maximum representation size admitted by the optional compiler-constant merge strategy.
/// @details This is intentionally much smaller than the general print stack budget. The strategy exists to fuse short
///          fields; turning a large scatter into a reserve proxy would replace a zero-copy write with materialization.
inline constexpr ::std::size_t compiler_constant_materialization_max_bytes{256u};

/// @brief Extracts the reserve proxy produced by compiler-constant materialization.
/// @details The proxy is a print/concat strategy object.  Its ordinary reserve CPOs are deliberately separate from the
///          source type's mature run-time formatter, so selecting the optional strategy cannot perturb that formatter's
///          inlining, code layout, or algorithm choice.
template <::std::integral char_type, typename T>
using compiler_constant_materialized_t = ::std::remove_cvref_t<decltype(
	print_compiler_constant_materialize(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
		::std::declval<T const &>()))>;

} // namespace details

/// @brief Opts one normalized leaf into print/concat's compiler-constant materialization strategy.
/// @details `print_compiler_constant_materialization_eligible` is an optimizer query, not a language constant-expression
///          promise: a GCC/Clang implementation may use `__builtin_constant_p` on the current object.  It must return
///          false whenever the companion materializer would not be a semantics-preserving replacement for this value.
///          The materializer returns a reserve-printable proxy whose formatter is intended only for the proven constant
///          arm.  Public print and concat require the complete active leaf run to satisfy this protocol and enter that
///          arm only when every query is true; the false arm invokes the pre-existing dispatcher unchanged.
///
///          Both customization functions are observational strategy queries: they must be side-effect free, must not
///          read volatile state, and must not rely on their evaluation order. The materializer must return its decayed
///          proxy as an exact prvalue. Its reserve extent is a type-level constant and the proxy must be nothrow
///          destructible, so merely forming the discarded constant arm cannot make an ordinary run-time call ill-formed.
/// @fn print_compiler_constant_materialization_eligible
/// @fn print_compiler_constant_materialize
template <typename char_type, typename T>
concept compiler_constant_printable =
	::std::integral<char_type> && requires(T const &value) {
		{
			print_compiler_constant_materialization_eligible(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value)
		} noexcept -> ::std::same_as<bool>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_compiler_constant_materialization_eligible(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value));
		{
			print_compiler_constant_materialize(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value)
		} noexcept -> ::std::same_as<
			::fast_io::details::compiler_constant_materialized_t<char_type, T>>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_compiler_constant_materialize(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value));
	} && reserve_printable<char_type,
							 ::fast_io::details::compiler_constant_materialized_t<char_type, T>> &&
	::std::is_nothrow_destructible_v<
		::fast_io::details::compiler_constant_materialized_t<char_type, T>> &&
	requires(
		::fast_io::details::compiler_constant_materialized_t<char_type, T> materialized,
		char_type *iter) {
		sizeof(::fast_io::details::compiler_constant_materialized_t<char_type, T>);
		typename ::std::integral_constant<
			::std::size_t,
			print_reserve_size(
				::fast_io::io_reserve_type<
					char_type,
					::fast_io::details::compiler_constant_materialized_t<char_type, T>>) >;
		{
			print_reserve_define(
				::fast_io::io_reserve_type<
					char_type,
					::fast_io::details::compiler_constant_materialized_t<char_type, T>>,
				iter, materialized)
		} noexcept -> ::std::same_as<char_type *>;
		// The speculative constant arm may be discarded; admitting either failure channel would change program effects.
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_reserve_define(
			::fast_io::io_reserve_type<
				char_type,
				::fast_io::details::compiler_constant_materialized_t<char_type, T>>,
			iter, materialized));
	};

/// @brief Default materializer for a compiler-constant eligibility gate which core has already observed as true.
/// @details Core print/concat call this internal protocol CPO only inside the matching true arm. Most sources need no
///          additional proof-sensitive work and forward directly to their ordinary materializer. A semantic source may
///          provide a more specialized overload when its ordinary, independently callable materializer must retain a
///          defensive value check. Keep this default forwarding leaf at ordinary placement: GCC 11--16 and Clang 17--23
///          produce identical print/concat wrappers and section sizes with or without forced inlining for integer,
///          floating, width, and ISO sources. `-O3` therefore already places this trivial leaf correctly; an attribute
///          would constrain newer optimizers without a measured code-generation benefit.
template <::std::integral char_type, typename T>
	requires ::fast_io::compiler_constant_printable<char_type, T>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type, T>, T const &value) noexcept
{
	return print_compiler_constant_materialize(
		::fast_io::io_reserve_type<char_type, T>, value);
}

/// @brief Marks an eligibility query as safe for direct optimizer evaluation at the consumer-owned expression boundary.
/// @details `__builtin_constant_p` does not generally look through an arbitrary call. This optional type-only CPO is
///          therefore a strong expression contract: every value-bearing field must be tested directly and every
///          independently unknown field must make the complete query fold to false with no surviving work. The shared
///          print/concat/to caller boundary is responsible for any measured mandatory inlining needed to expose the
///          original expression. The provider query itself remains ordinary inline unless an isolated A/B proves that
///          a narrower attribute is required; imposing `always_inline` merely because this marker exists would enlarge
///          every future consumer without proving a deletion benefit. Marked queries may be evaluated directly;
///          unmarked extensions retain the conservative builtin-on-call-expression gate.
/// @fn print_compiler_constant_materialization_query_inline_safe
template <typename char_type, typename T>
concept compiler_constant_query_inline_safe =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_materialization_query_inline_safe(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Opts a public source leaf into compiler-constant materialization before print aliasing.
/// @details The ordinary compiler-constant protocol is phrased on a printable leaf. Public print, however, receives
///          the source object before `io_print_alias` and status forwarding. This stronger marker states that replacing
///          that exact source with its companion materialized proxy before those customizations is observationally
///          equivalent. It is intentionally type-only and exact-`true_type`: arbitrary alias/status extensions must
///          keep the historical normalization boundary unless they explicitly establish this stronger contract.
///
///          The marker also promises that the source's eligibility query is pure and expression-transparent under the
///          consumer-owned query boundary. Every unknown payload field must therefore fold the query to false without
///          leaving run-time work. This is what permits the false arm of the public early gate to be the unchanged print
///          entry; placement of the provider function itself remains governed by the measured rule above.
/// @fn print_compiler_constant_pre_normalization_safe
template <typename char_type, typename T>
concept compiler_constant_pre_normalization_safe =
	::std::integral<char_type> &&
	::fast_io::compiler_constant_printable<char_type, T> &&
	::fast_io::compiler_constant_query_inline_safe<char_type, T> && requires {
		{
			print_compiler_constant_pre_normalization_safe(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a compiler-constant provider whose complete optimizer graph has a permanent deletion classification.
/// @details The semantic replacement contract above is intentionally insufficient for code-generation admission. A
///          provider may satisfy every observable-output requirement while its query, materializer, or proxy writer
///          remains as run-time work after `__builtin_constant_p` succeeds. This exact-`true_type` marker states that
///          the provider has permanent constant/unknown query roots for every value-bearing field and recursive
///          assembly/IR evidence for its declared source-shape categories. A constant root must erase the query,
///          materializer, proxy writer, and native formatter selected by the audited consumer; an independently unknown
///          field must fold the query to false and retain the ordinary native continuation without proxy residue.
///
///          This is only the provider half of the proof. Each print, concat, or conversion consumer must still combine
///          the marker with its own destination, record-shape, compiler, and version deletion proof before evaluating a
///          value query. A provider extension without this marker therefore remains semantically usable but is
///          fail-closed for the optional optimization. Supplying the marker without the stated paired evidence violates
///          the concept contract; returning `bool`, another truth-valued type, or deriving it from a value query is not
///          sufficient. The CPO must be type-only so a rejecting consumer can inspect it before forming a replacement.
/// @fn print_compiler_constant_materialization_graph_proven
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_materialization_graph_proven_source_shape =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_materialization_graph_proven(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Combines a provider's code-generation classification with its complete semantic replacement protocol.
/// @details Consumers which have already selected a supported compiler/destination category may use this conjunction;
///          the source-shape form remains available for earlier fail-closed ordering.
template <typename char_type, typename T>
concept compiler_constant_materialization_graph_proven =
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char_type, T> &&
	::fast_io::compiler_constant_printable<char_type, T>;

/// @brief Marks a flat scalar source whose compiler-constant graph has no nested semantic or precision state.
/// @details The provider promises that the source query observes only the scalar payload represented by this source and
///          that successful materialization produces one flat replacement leaf with the same spelling. The replacement
///          may expose reserve, precise, or immutable-fragment protocols, but it must not contain a condition, pack,
///          dynamic width/precision owner, or another value-dependent operation graph.
///
///          This marker is deliberately independent from `compiler_constant_pre_normalization_safe`. A
///          compiler-specific consumer can reject an unsupported source shape before asking whether the source owns the
///          replacement protocol at all. Passing this marker proves shape only: the consumer must then establish the
///          complete pre-normalization contract and prove, for its exact compiler/version, record shape, and destination
///          strategy, that a successful query erases the complete replacement formatter. The marker CPO's own
///          constraints must depend only on primitive source traits; depending on this full concept, the candidate
///          predicate, or a replacement type would create a recursive satisfaction graph and violate fail-closed order.
/// @fn print_compiler_constant_simple_scalar_source
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_simple_scalar_source_shape =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_simple_scalar_source(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Combines the flat-scalar shape marker with the complete source replacement contract.
/// @details Consumers which do not need ordered fail-closed probing may use this conjunction directly. A positive result
///          still says nothing about code-generation profitability or deletion on a particular compiler.
template <typename char_type, typename T>
concept compiler_constant_simple_scalar_source =
	::fast_io::compiler_constant_simple_scalar_source_shape<char_type, T> &&
	::fast_io::compiler_constant_pre_normalization_safe<char_type, T>;

/// @brief Marks a compiler-constant source graph which owns run-time floating precision state.
/// @details The provider promises that the source, a spelling wrapper, or an exact semantic/layout wrapper contains a
///          descendant with both a floating payload and a value-dependent precision payload, and that materializing the
///          complete graph constructs the descendant's precision plan. A wrapper may forward this marker only when its
///          ordinary execution necessarily retains that exact child graph; unrelated siblings or inactive condition
///          arms are insufficient. This is a negative code-generation partition, not permission to materialize:
///          consumers use it to reject compiler/version paths which retain the planner after
///          `__builtin_constant_p` succeeds. The marker must be type-only and must not evaluate either payload, form a
///          replacement, or claim output equivalence.
/// @fn print_compiler_constant_dynamic_precision_floating_leaf
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_dynamic_precision_floating_source_shape =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_dynamic_precision_floating_leaf(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a source whose ordinary spelling is one borrowed text range.
/// @details This is a source-shape fact, not permission to replace the source.
///          The provider promises that ordinary normalization borrows the exact
///          `[data, data + size)` character sequence for the complete synchronous
///          operation and adds no formatting state. A consumer may use the fact
///          to reject compiler-constant materialization when that strategy would
///          only copy the same range into automatic storage. The marker is
///          type-only; it must not read the source, invoke its alias, or claim
///          that its characters are constant.
/// @fn print_compiler_constant_borrowed_text_leaf
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_borrowed_text_source_shape =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_borrowed_text_leaf(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Describes the rodata/static fragments of one compiler-constant replacement proxy.
/// @details This protocol is deliberately independent from reserve printing.  A reserve formatter writes characters
///          into caller-owned storage; the fragment formatter instead fills only scatter descriptors whose payloads
///          already reside in immutable storage with lifetime covering the complete synchronous write.  It is used by
///          unbuffered print destinations so constant digits, boolean spellings, and punctuation need not be copied to
///          a character array merely to issue one write/writev operation.
///
///          `print_compiler_constant_static_fragments_size` is a type-only maximum descriptor count.  The define CPO
///          may return any cursor in the closed range `[first, first + maximum]`, omitting empty fragments.  It must not
///          write character payload, retain `first`, or return descriptors naming the proxy object itself.  The proxy
///          may be destroyed immediately after the synchronous output operation; every descriptor payload must remain
///          valid independently of that destruction.  These requirements are stronger than ordinary scatter
///          printability and intentionally keep short-lived/local scratch pointers out of the static-fragment path.
/// @fn print_compiler_constant_static_fragments_size
/// @fn print_compiler_constant_static_fragments_define
template <typename char_type, typename T>
concept compiler_constant_static_fragment_printable =
	::std::integral<char_type> && requires(
		::fast_io::basic_io_scatter_t<char_type> *first,
		::std::remove_cvref_t<T> const &value) {
		typename ::std::integral_constant<
			::std::size_t,
			print_compiler_constant_static_fragments_size(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>) >;
		{
			print_compiler_constant_static_fragments_define(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>,
				first, value)
		} noexcept -> ::std::same_as<
			::fast_io::basic_io_scatter_t<char_type> *>;
		// Fragment discovery is a strategy proof and cannot publish a recoverable failure to its caller.
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_compiler_constant_static_fragments_define(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, first, value));
	} &&
	(print_compiler_constant_static_fragments_size(
		 ::fast_io::io_reserve_type<char_type,
			 ::std::remove_cvref_t<T>>) != 0u);

/// @brief Marks a proxy whose immutable fragments are the preferred bounded mid-end materialization spelling.
/// @details This marker strengthens `compiler_constant_static_fragment_printable` for consumers which need one
///          contiguous character range rather than scatter output. The provider promises that zero-initializing the
///          declared maximum descriptor array, invoking the fragment define CPO once, and copying every nonempty slot
///          in index order produces exactly the ordinary reserve spelling and no more than its type-level reserve
///          extent. It also promises that unused slots may remain zero descriptors and that expanding the bounded slots
///          does not add observable work.
///
///          A consumer may select this spelling only after a compiler-specific deletion test proves that the provider's
///          exact writer would retain run-time formatting work and that expanded fragment copying is fully eliminated
///          for a successful optimizer query. Unknown inputs must fail before proxy construction, and a consumer must
///          still bound the aggregate descriptor count and character extent. The marker is therefore a formal strategy
///          preference, not permission to allocate an unbounded descriptor or character array.
/// @fn print_compiler_constant_prefer_expanded_fragments
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_expanded_fragment_preferred =
	::std::integral<char_type> &&
	::fast_io::compiler_constant_static_fragment_printable<char_type, T> &&
	::fast_io::reserve_printable<char_type, T> && requires {
		{
			print_compiler_constant_prefer_expanded_fragments(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Classifies the source shape of a replacement which prefers expanded immutable fragments.
/// @details This source-side marker lets a consumer reject an unaudited code-generation strategy before forming the
///          replacement type or evaluating its value query. The provider promises that a successful materialization of
///          this exact source produces a replacement satisfying `compiler_constant_expanded_fragment_preferred` for the
///          same character type. The marker is type-only and has no output, aliasing, or status effect. Its CPO
///          constraints must depend only on primitive source traits; referring to the complete safety contract would
///          make an early fail-closed consumer recursively instantiate the replacement protocol it is meant to avoid.
///
///          The marker does not authorize fragment materialization. A consumer may use it only as an early strategy
///          partition: either its complete compiler/version deletion proof covers the expanded-fragment graph, or it
///          must fail closed before `__builtin_constant_p`. This distinction is needed when one IO destination erases a
///          provider graph while another retains descriptor traversal or formatting work.
/// @fn print_compiler_constant_source_prefer_expanded_fragments
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_expanded_fragment_preferred_source_shape =
	::std::integral<char_type> && requires {
		{
			print_compiler_constant_source_prefer_expanded_fragments(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Combines the expanded-fragment source marker with the complete replacement contract.
/// @details This conjunction is appropriate only after a consumer's compiler partition has admitted the source shape.
///          It proves semantic materialization safety, but does not prove that any compiler erases the formatter.
template <typename char_type, typename T>
concept compiler_constant_expanded_fragment_preferred_source =
	::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char_type, T> &&
	::fast_io::compiler_constant_pre_normalization_safe<char_type, T>;

/// @brief Opts an output into synchronous, direct consumption of one immutable scalar range.
/// @details A scalar write customization proves only that an output accepts a pointer range. It does not prove that
///          the output is unbuffered or that the pointed-to storage is no longer observed after the customization
///          returns. Buffered streams, converting decorators, deferred/asynchronous adapters, and type-erased wrappers
///          therefore remain outside this concept unless the concrete normalized output explicitly supplies this
///          stronger marker.
///
///          An opt-in promises that the matching native typed- or byte-write operation consumes the complete source
///          range synchronously during the call and does not retain its pointer after returning. It also promises that
///          bypassing an intermediate character copy is observationally equivalent to the ordinary print path. A
///          transparent wrapper may forward the marker only when it preserves all of those properties; merely
///          forwarding `write_*` is insufficient.
/// @fn print_synchronous_direct_scalar_output
/// @return std::true_type
template <typename char_type, typename output>
concept synchronous_direct_scalar_output =
	::std::integral<char_type> && requires {
		{
			print_synchronous_direct_scalar_output(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<output>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Opts an output into synchronous, direct consumption of immutable scatter payloads.
/// @details A scatter-write customization proves only that an output accepts a descriptor array.  It does not prove
///          that the output is unbuffered, that it consumes every payload before returning, or that it forwards the
///          descriptor pointers unchanged to the underlying device.  Buffered streams, put-area streams, converting
///          decorators, deferred/asynchronous adapters, and type-erased wrappers therefore remain outside this concept
///          unless the concrete normalized output explicitly supplies this stronger marker.
///
///          An opt-in promises that the matching native typed- or byte-scatter operation consumes the referenced
///          character storage synchronously during the call and does not retain a pointer after it returns.  It also
///          promises that bypassing an intermediate character copy is observationally equivalent to the ordinary
///          print path.  A transparent wrapper may forward the marker only when it preserves all of those properties;
///          merely forwarding `scatter_write_*` is insufficient.
/// @fn print_synchronous_direct_scatter_output
/// @return std::true_type
template <typename char_type, typename output>
concept synchronous_direct_scatter_output =
	::std::integral<char_type> && requires {
		{
			print_synchronous_direct_scatter_output(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<output>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Supplies the conservative staged-fallback placement when a type does not customize it.
/// @details This platform-neutral default preserves the original cold fallback for every existing staged formatter.
///          A type may provide a more specialized overload in an associated namespace when measurements prove that
///          in-caller fallback is profitable. Keeping the default as an ordinary CPO overload preserves ADL and
///          avoids making a newly introduced scheduling hint a source-breaking requirement of `staged_printable`.
template <::std::integral char_type, typename T>
inline constexpr bool print_staged_fallback_inline(io_reserve_type_t<char_type, T>) noexcept
{
	return false;
}

/// @brief Supplies the unbounded staged-group default for types without a measured register-pressure limit.
/// @details `print_staged_width` remains the minimum profitable count.  This independent upper bound lets a
///          customization reject larger complete groups when its prepared state stops fitting the target register
///          budget.  The default preserves the historical prepare-the-complete-run behavior of every existing staged
///          formatter.  A finite customization changes scheduling admission only; ordinary reserve formatting remains
///          the exact fallback for a larger group.
template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_staged_max_count(io_reserve_type_t<char_type, T>) noexcept
{
	return SIZE_MAX;
}

namespace details
{

template <::std::size_t>
struct compile_time_size_constant
{};

// Forming a class-template specialization from a policy value proves that the value is a constant expression. This
// check is intentionally shared by stack, heap, descriptor-count, and profitability policies; the old name remains
// as an alias because existing customization points use it as an implementation detail.
template <::std::size_t value>
using reserve_static_stack_size_constant = compile_time_size_constant<value>;

template <::std::integral char_type, typename T>
using staged_printable_state_t = typename ::std::remove_cvref_t<decltype(print_staged_type(
	io_reserve_type<char_type, ::std::remove_cvref_t<T>>))>::type;

/// @brief Proves that a staged state can cross both preparation and no-throw array storage.
/// @details The dispatcher default-constructs a state array and assigns each prepared value into its slot. Completeness
///          must be checked before the standard construction/assignment concepts: libc++ and libstdc++ may implement
///          those concepts with traits that issue a hard diagnostic for a forward declaration instead of yielding
///          false during constraint substitution. Both array construction and the preparation-store helper are
///          strategy-introduced work. Admitting a throwing default constructor would add an exception absent from the
///          ordinary formatter, while admitting a throwing assignment under the helper's `noexcept` contract would
///          turn it into `terminate`. Both operations must therefore be non-throwing before optional staging is selected.
template <typename state_type>
inline consteval bool staged_printable_state_object_impl() noexcept
{
	using value_type = ::std::remove_cvref_t<state_type>;
	if constexpr (!::std::same_as<state_type, value_type> || !::std::is_object_v<state_type> ||
				  ::std::is_array_v<state_type> || !requires { sizeof(value_type); })
	{
		return false;
	}
	else
	{
		constexpr bool traditional_nothrow{
			::std::default_initializable<value_type> &&
			::std::is_nothrow_default_constructible_v<value_type> &&
			::std::assignable_from<value_type &, value_type> &&
			::std::is_nothrow_assignable_v<value_type &, value_type>};
#if defined(__HERBCEPTIONS__)
		// The standard nothrow traits observe only unwinding. Staged array construction and assignment occur inside a
		// no-failure region, so a deterministic constructor/assignment channel must independently be absent as well.
		return traditional_nothrow &&
			   !::std::is_herbceptions_throws_constructible_v<value_type> &&
			   !::std::is_herbceptions_throws_assignable_v<value_type &, value_type>;
#else
		return traditional_nothrow;
#endif
	}
}

template <::std::integral char_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t dynamic_reserve_default_static_stack_size() noexcept
{
	constexpr ::std::size_t bytes{::fast_io::details::print_stack_buffer_max_bytes<stack_policy>()};
	if constexpr (bytes == 0)
	{
		return 0u;
	}
	else if constexpr (sizeof(char_type) < bytes)
	{
		return bytes / sizeof(char_type);
	}
	else
	{
		return 1u;
	}
}

} // namespace details

/// @warning    UNSTABLE
/// @brief      staged_printable
/// @details    Describes a type whose preparation can be separated from emission. A print run may prepare several
///             independent values before emitting them in order to expose instruction-level parallelism. Buffer
///             sizing and non-staged fallback are orthogonal printing capabilities selected by the caller.
///             `print_staged_width` is the minimum compatible staged-argument count that makes this scheduling
///             strategy available; it is not a preferred or fixed batch size. Once the threshold is met, the caller
///             may prepare and emit the complete compatible run, including counts that are not multiples of the
///             threshold.
/// @fn         print_staged_type
/// @brief      Indicates the prepared state type returned by print_staged_prepare.
/// @fn         print_staged_width
/// @brief      Returns the nonzero compile-time minimum argument count that enables a staged run.
/// @fn         print_staged_fallback_inline
/// @brief      Reports whether the ordinary fallback should remain in the staged caller.
/// @details    The platform-neutral default is `false`, so existing staged formatters retain the cold fallback without
///             adding a customization. A type may return `true` to keep a performance-audited fallback adjacent to
///             the eligibility branch. The policy does not alter formatting, capacity, or argument order.
/// @fn         print_staged_max_count
/// @brief      Returns the nonzero compile-time maximum compatible staged-argument count admitted by this strategy.
/// @details    The default is unbounded. A finite value must not be smaller than `print_staged_width`; it permits a
///             formatter to retain scalar emission once a larger prepared array would exceed its measured register
///             budget. This is an admission policy, not a request to split a larger run into observable sub-runs.
/// @fn         print_staged_eligible
/// @brief      Reports whether an object may use the staged path.
/// @fn         print_staged_prepare
/// @brief      Prepares one object without emitting characters.
/// @fn         print_staged_define
/// @brief      Emits one eligible object from its prepared state into a caller-provided contiguous buffer.
template <typename char_type, typename T>
concept staged_printable =
	::std::integral<char_type> && requires(T const &t, char_type *ptr) {
		typename ::fast_io::details::staged_printable_state_t<char_type, T>;
		{
			print_staged_type(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} noexcept;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(
			print_staged_type(io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		requires ::fast_io::details::staged_printable_state_object_impl<
			::fast_io::details::staged_printable_state_t<char_type, T>>();
		{
			print_staged_width(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} noexcept -> ::std::same_as<::std::size_t>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(
			print_staged_width(io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		typename ::std::integral_constant<::std::size_t, print_staged_width(
														 io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		requires(print_staged_width(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u);
		{
			print_staged_fallback_inline(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} noexcept -> ::std::same_as<bool>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(
			print_staged_fallback_inline(io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		typename ::std::integral_constant<bool, print_staged_fallback_inline(
										 io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			print_staged_max_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} noexcept -> ::std::same_as<::std::size_t>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(
			print_staged_max_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		typename ::std::integral_constant<::std::size_t, print_staged_max_count(
											  io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		requires(print_staged_max_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) >=
				 print_staged_width(io_reserve_type<char_type, ::std::remove_cvref_t<T>>));
		requires requires(::fast_io::details::staged_printable_state_t<char_type, T> const &state) {
			{
				print_staged_eligible(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
			} noexcept -> ::std::same_as<bool>;
			requires FAST_IO_HERBCEPTIONS_NOTHROWS(
				print_staged_eligible(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t));
			{
				print_staged_prepare(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
			} noexcept -> ::std::same_as<::fast_io::details::staged_printable_state_t<char_type, T>>;
			requires FAST_IO_HERBCEPTIONS_NOTHROWS(
				print_staged_prepare(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t));
			{
				print_staged_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t, state)
			} noexcept -> ::std::same_as<char_type *>;
			// Prepared state is emitted inside a no-failure scheduling region; inspect both exception mechanisms.
			requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_staged_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t, state));
		};
	};

/// @brief      dynamic_reserve_with_possible_static_stack_size
/// @details    A dynamic-reserve printable may advertise a constexpr preferred local-buffer capacity for small
///             materializations. This is a cost hint, not a proof that every object fits and not permission to add
///             the value to every other alternative's stack frame. Dispatch must first measure the object and use a
///             non-stack path when the measured capacity exceeds the hint. Zero disables the local-buffer path;
///             `SIZE_MAX` is reserved as an invalid/unbounded sentinel.
/// @fn         print_reserve_static_stack_size
/// @brief      Returns the preferred local-buffer capacity, in char_type units.
template <typename char_type, typename T>
concept dynamic_reserve_with_possible_static_stack_size =
	::std::integral<char_type> && dynamic_reserve_printable<char_type, T> && requires {
		{
			print_reserve_static_stack_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		typename ::fast_io::details::reserve_static_stack_size_constant<print_reserve_static_stack_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		requires(print_reserve_static_stack_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != SIZE_MAX);
	};

/// @warning    UNSTABLE
/// @brief      context_printable
/// @details    That a type is context printable
///             is that it can be partially printed to a buffer multiple times.
/// @fn         print_context_type
/// @brief      Indicates the type of your context object for context printable types.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, T>  tag-invoke
/// @return     ::fast_io::io_type_t<your_context_type>     the type of the context object
/// @struct     your_context_type
/// @brief      The context object, usually stores the progress of printing.
/// @fn         print_context_define
/// @brief      Partially prints an object to a buffer with a context object.
/// @tparam     <auto-inferred>
/// @param      this                                        the context object
/// @param      T                                           the object to be printed
/// @param      char_type*                                  a pointer to the beginning of the buffer
/// @param      char_type*                                  a pointer to the end of the buffer
/// @return     ::fast_io::context_print_result<char_type*> a pointer to the next after printing
///                                                         and a boolean indicating whether the printing is done
/// @note       The advertised state must be an unqualified, non-array, default-initializable object. The print layer
///             owns one mutable state for the complete incremental operation; cv-qualified and adjusted array states
///             cannot implement that lifetime contract.
template <typename char_type, typename T>
concept context_printable = ::std::integral<char_type> && requires(T t, char_type *ptr) {
	requires requires(::fast_io::details::print_context_state_t<char_type, T> &st) {
		requires ::fast_io::details::print_context_state_object<
			::fast_io::details::print_context_state_t<char_type, T>>;
		{ st.print_context_define(t, ptr, ptr) } -> ::std::same_as<context_print_result<char_type *>>;
	};
};

/// @brief      context_printable_with_static_buffer_size
/// @details    That a context printable type declares a constexpr contiguous
///             buffer window size that callers may use to drive the producer.
///             The value is a stack/local streaming window in char_type units,
///             not a total output size. Multiple context producers should share
///             a window by taking the maximum required size, not by summing the
///             returned values. Opt-in producers should keep this value stack-safe;
///             zero, `SIZE_MAX`, and values outside the pointer-difference domain
///             do not form this optional capability.
/// @fn         print_context_static_buffer_size
/// @brief      Returns the static context buffer window size, in char_type units.
template <typename char_type, typename T>
concept context_printable_with_static_buffer_size =
	::std::integral<char_type> && context_printable<char_type, T> && requires {
		{
			print_context_static_buffer_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		typename ::fast_io::details::reserve_static_stack_size_constant<print_context_static_buffer_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		requires(print_context_static_buffer_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0);
		requires(print_context_static_buffer_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != SIZE_MAX);
		requires(print_context_static_buffer_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <
				 static_cast<::std::size_t>(PTRDIFF_MAX));
	};

/// @brief      scatter_fallback_full_output_threshold_stream
/// @details    Customizes the maximum full-output size, in `char_type` units, for coalescing an emulated scatter
///             operation into contiguous storage before one write. It applies only when the stream has no native
///             scatter operation: without coalescing, N descriptors become as many as N ordinary writes, so copying
///             can remove syscall or virtual-call overhead. This is a distinct cost question from replacing one
///             native scatter call with one write. The value is required to be a constant expression; zero disables
///             the path. Storage selection remains the print layer's responsibility.
/// @fn         scatter_fallback_full_output_threshold
/// @brief      Returns the scatter coalescing threshold, in char_type units.
template <typename char_type, typename T>
concept scatter_fallback_full_output_threshold_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_fallback_full_output_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_fallback_full_output_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};
/// @brief      scatter_direct_full_output_coalesce_threshold_stream
/// @details    Customizes the maximum complete scatter payload size, in `char_type` units, for replacing one native
///             scatter operation with one contiguous write after copying every element. The saved work is descriptor
///             setup and scatter-call overhead, not N writes; therefore this threshold must remain independent of
///             `scatter_fallback_full_output_threshold`. The value is a constant expression and zero disables the
///             path.
/// @fn         scatter_direct_full_output_coalesce_threshold
/// @brief      Returns the native-scatter-to-write coalescing threshold.
template <typename char_type, typename T>
concept scatter_direct_full_output_coalesce_threshold_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_direct_full_output_coalesce_threshold(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_direct_full_output_coalesce_threshold(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief      full_output_coalesce_threshold_stream
/// @details    Customizes the maximum full semantic output size, in `char_type` units, that may be precisely measured
///             and materialized in stack-bounded contiguous storage before one write. This policy describes semantic
///             print composition, whereas the two scatter policies above describe an already-built descriptor list;
///             sharing a threshold would conflate different copying and call costs. The value is a constant
///             expression and zero disables the path.
/// @fn         full_output_coalesce_threshold
/// @brief      Returns the whole-output coalescing threshold, in char_type units.
template <typename char_type, typename T>
concept full_output_coalesce_threshold_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			full_output_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			full_output_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief      full_output_dynamic_coalesce_threshold_stream
/// @details    Customizes the maximum full semantic output size, in `char_type` units, that may be materialized in
///             dynamically allocated contiguous storage before one output operation. It is deliberately separate
///             from `full_output_coalesce_threshold`: heap allocation changes both the fixed cost and the acceptable
///             size range, and a large heap threshold must never silently enlarge a stack frame. The value is a
///             constant expression and zero disables dynamic whole-output materialization.
/// @fn         full_output_dynamic_coalesce_threshold
/// @brief      Returns the dynamic whole-output coalescing threshold.
template <typename char_type, typename T>
concept full_output_dynamic_coalesce_threshold_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			full_output_dynamic_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			full_output_dynamic_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief Cost policy for repeated semantic fill output.
/// @details All sizes are expressed in `char_type` units except `preferred_scatter_count`, which counts descriptors.
///          A repeated fill has unusual economics: one initialized block can represent arbitrarily large padding,
///          but too many references to that block create descriptor and syscall pressure. Keeping allocation,
///          block-size, and batching decisions in one policy prevents individually reasonable values from composing
///          into an invalid or pathologically expensive plan.
struct repeated_fill_output_policy
{
	/// Largest fill that may be materialized as one dynamically allocated contiguous payload; zero disables it.
	::std::size_t dynamic_coalesce_threshold;
	/// Number of reusable fill characters represented by one scatter descriptor; required to be nonzero.
	::std::size_t scatter_block_size;
	/// Preferred descriptor batch size before the native stream limit is applied; required to be nonzero.
	::std::size_t preferred_scatter_count;
};

/// @brief      repeated_fill_output_policy_stream
/// @details    Each member is validated as a constant expression so dispatch, storage layout, and batching remain
///             compile-time decisions. The two divisor/count members are nonzero by contract; zero remains meaningful
///             only for disabling dynamic coalescing.
/// @fn         repeated_fill_output_policy_define
/// @brief      Returns the stream's compile-time repeated-fill cost policy.
template <typename char_type, typename T>
concept repeated_fill_output_policy_stream =
	::std::integral<char_type> && requires {
		{
			repeated_fill_output_policy_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::fast_io::repeated_fill_output_policy>;
		typename ::fast_io::details::compile_time_size_constant<repeated_fill_output_policy_define(
																	io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
																	.dynamic_coalesce_threshold>;
		typename ::fast_io::details::compile_time_size_constant<repeated_fill_output_policy_define(
																	io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
																	.scatter_block_size>;
		typename ::fast_io::details::compile_time_size_constant<repeated_fill_output_policy_define(
																	io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
																	.preferred_scatter_count>;
		requires(repeated_fill_output_policy_define(
					 io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .scatter_block_size != 0u);
		requires(repeated_fill_output_policy_define(
					 io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .preferred_scatter_count != 0u);
	};

/// @brief      scatter_read_maximum_count_stream
/// @details    Customizes the maximum descriptor count accepted by one native scatter-read operation. The count
///             applies equally to character and byte descriptors and to read and positional-read variants. Generic
///             all-operations partition a larger list into legal nonempty prefixes; generic some-operations expose at
///             most one such prefix. A completed zero-length descriptor is still descriptor progress, so a zero-byte
///             native result over an all-empty admitted prefix denotes `{prefix_count, 0}`, not end of file. The value
///             is a nonzero constant expression.
/// @fn         scatter_read_maximum_count
/// @brief      Returns the maximum descriptors accepted by one scatter-read operation.
template <typename char_type, typename T>
concept scatter_read_maximum_count_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_read_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_read_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		requires(scatter_read_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u);
	};

namespace details
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t scatter_read_maximum_count_or_unlimited() noexcept
{
	if constexpr (::fast_io::scatter_read_maximum_count_stream<char_type, T>)
	{
		return scatter_read_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>);
	}
	else
	{
		// SIZE_MAX is only the generic dispatcher's "no declared limit" sentinel; a platform adapter remains free to
		// impose a stricter trust-boundary limit before entering its native API.
		return SIZE_MAX;
	}
}

} // namespace details

/// @brief      scatter_write_maximum_count_stream
/// @details    Customizes the maximum descriptor count accepted by one native scatter-write operation. The count
///             applies equally to character and byte descriptors and to write and positional-write variants. Generic
///             all-operations partition a larger list into legal nonempty prefixes; generic some-operations expose at
///             most one such prefix. Consequently every forwarded native call satisfies `0 < count <= maximum`, and
///             concatenating the prefixes preserves the original descriptor order. The value is a nonzero constant
///             expression.
/// @fn         scatter_write_maximum_count
/// @brief      Returns the maximum descriptors accepted by one scatter operation.
template <typename char_type, typename T>
concept scatter_write_maximum_count_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_write_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_write_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		requires(scatter_write_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != 0u);
	};

namespace details
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t scatter_write_maximum_count_or_unlimited() noexcept
{
	if constexpr (::fast_io::scatter_write_maximum_count_stream<char_type, T>)
	{
		return scatter_write_maximum_count(io_reserve_type<char_type, ::std::remove_cvref_t<T>>);
	}
	else
	{
		// SIZE_MAX is a local "no library-imposed limit" sentinel. It is never passed to an operating-system API as a
		// descriptor count; platform adapters may impose a stricter limit before the call.
		return SIZE_MAX;
	}
}

} // namespace details

/// @brief      small_scatter_coalesce_threshold_stream
/// @details    Opts a stream into small-scatter repacking and customizes the maximum individual scatter size, in
///             `char_type` units, eligible for copying. Eligibility is intentionally separate from temporary storage
///             capacity and minimum descriptor savings: element size, available chunk storage, and profitability are
///             independent dimensions. Returning zero disables the path. Streams should opt in only when reducing
///             native descriptor pressure is known to repay the extra copy.
/// @fn         small_scatter_coalesce_threshold
/// @brief      Returns the small-scatter element threshold, in char_type units.
template <typename char_type, typename T>
concept small_scatter_coalesce_threshold_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			small_scatter_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			small_scatter_coalesce_threshold(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief      scatter_repack_chunk_size_stream
/// @details    Customizes the temporary chunk capacity, in `char_type` units, used to replace several small native
///             scatter elements with one descriptor. This bounds storage for one repacked run; it is neither a
///             complete-output threshold nor a promise that all eligible elements fit. Returning zero disables native
///             scatter repacking for the stream.
/// @fn         scatter_repack_chunk_size
/// @brief      Returns the native scatter repack chunk capacity.
template <typename char_type, typename T>
concept scatter_repack_chunk_size_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_repack_chunk_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_repack_chunk_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief      scatter_repack_minimum_saved_scatter_count_stream
/// @details    Customizes how many native scatter descriptors a repack plan must eliminate before its copying cost is
///             accepted. The value is a profitability threshold measured in descriptors, not characters or bytes;
///             keeping that unit explicit prevents accidental comparison with either repack capacity.
/// @fn         scatter_repack_minimum_saved_scatter_count
/// @brief      Returns the minimum profitable native scatter reduction.
template <typename char_type, typename T>
concept scatter_repack_minimum_saved_scatter_count_stream =
	::std::integral<char_type> && requires {
		typename ::fast_io::details::compile_time_size_constant<
			scatter_repack_minimum_saved_scatter_count(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		{
			scatter_repack_minimum_saved_scatter_count(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
	};

/// @brief      printable_internal_shift
/// @details    Defines the behaviour when printed with ::fast_io::mnp::width<::fast_io::mnp::scalar_placement::internal>
/// @fn         print_define_internal_shift
/// @brief      Returns the number of the characters printed on the left of the filling spaces.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<expression_type>> tag-invoke
/// @param      T                                           the object to be printed
/// @return     ::std::size_t                               the number of characters appaired on the left
template <typename char_type, typename T>
concept printable_internal_shift = ::std::integral<char_type> && requires(T t) {
	{
		print_define_internal_shift(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
	} -> ::std::same_as<::std::size_t>;
};

/// @brief      precise_reserve_printable
/// @details    Refines `reserve_printable` or `dynamic_reserve_printable` with an object-dependent exact-length
///             protocol. Unlike an ordinary reserve bound, the precise size may be used for exact allocation and
///             whole-output coalescing. `print_reserve_precise_define` must emit exactly that many characters for the
///             same object and must preserve the ordinary formatting semantics.
/// @fn         print_reserve_precise_size
/// @brief      Returns the precise size of the buffer needed to print an object.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param      T                                           the object to be printed
/// @return     ::std::size_t                               the precise size of the buffer to be reserved
/// @fn         print_reserve_precise_define
/// @brief      Prints the object to a buffer with a precise size.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param      char_type*                                  a pointer to the beginning of the buffer
/// @param      ::std::size_t                               the size of the buffer
/// @param      T                                           the object to be printed
/// @return     void or char_type*; a pointer result reports the actual end, while void advances by the exact size
template <typename char_type, typename T>
concept precise_reserve_printable =
	::std::integral<char_type> &&
	(reserve_printable<char_type, T> || dynamic_reserve_printable<char_type, T>) && requires(T t, char_type *ptr, ::std::size_t n) {
		{
			print_reserve_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
		} -> ::std::same_as<::std::size_t>;
		// Every dispatcher has exactly two emission branches. Proving that disjunction here prevents an unrelated return
		// type from being admitted and then silently treated as `void`, which would discard the producer's actual cursor.
		requires(
			::std::same_as<decltype(print_reserve_precise_define(
							   io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, n, t)),
						   void> ||
			::std::same_as<decltype(print_reserve_precise_define(
							   io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, n, t)),
						   char_type *>);
	};

/// @brief Refines exact reserve formatting with a non-throwing, pointer-reporting emission expression.
/// @details Exact size alone is not sufficient inside a C++23 overwrite callback: an exception escaping the callback
///          does not have the ordinary concat strategy's simple partially-constructed-result contract. This concept
///          therefore tests the concrete named-lvalue expression used after phase-1 decay and requires both the
///          traditional `noexcept` result and the absence of a Herbception channel. Requiring the exact `char_type*`
///          result also lets the caller validate that the producer ended at its promised extent before the destination
///          publishes that extent. Void-returning precise producers remain valid `precise_reserve_printable`s, but
///          deliberately stay on the established strategy until a separate non-failing endpoint proof exists for them.
template <typename char_type, typename T>
concept nothrow_precise_reserve_printable =
	::std::integral<char_type> && precise_reserve_printable<char_type, T> &&
	requires(T &value, char_type *ptr, ::std::size_t n) {
		{
			print_reserve_precise_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, n, value)
		} noexcept -> ::std::same_as<char_type *>;
		// C++23 overwrite callbacks cannot carry either fast_io-supported failure channel across their ABI boundary.
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_reserve_precise_define(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, n, value));
	};

/// @brief Classifies a materialized compiler-constant proxy with one bounded integer conversion leaf.
/// @details This type-only provider contract is stronger than precise reserve printability. The proxy's complete exact
///          writer graph must consist only of integral sign/prefix/digit spelling whose loops are bounded by the
///          integer type, plus type-owned spelling wrappers. It must contain no floating-point precision planner,
///          dynamic width, semantic condition, allocation, or fallback to the corresponding native source writer.
///          Consumers may use the marker only after an independent value gate has selected the proxy; the marker does
///          not make an ordinary source eligible and does not authorize a formatting strategy.
/// @fn print_compiler_constant_flat_integer_replacement
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_flat_integer_replacement =
	::std::integral<char_type> &&
	::fast_io::nothrow_precise_reserve_printable<char_type, T> &&
	requires {
		{
			print_compiler_constant_flat_integer_replacement(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a compiler-constant proxy whose exact reserve spelling should be tried before immutable fragments.
/// @details Some proxies have a deliberately conservative static-fragment count even though their selected spelling
///          is short.  On synchronous direct outputs, building that worst-case descriptor array can cost much more
///          stack than writing the exact spelling contiguously.  This type-only opt-in is therefore a strong contract:
///          the exact-size query is non-throwing and stable for the lifetime of the proxy, and the exact writer writes
///          only `[ptr, ptr + n)`, returns `ptr + n`, and is semantically identical to both ordinary reserve output and
///          immutable-fragment output.  A consumer may query one proxy once, retain that result, and use it with the
///          same proxy after preparing sibling leaves; it must not substitute the ordinary reserve writer because that
///          writer may legally use the complete reserve bound.
/// @fn print_compiler_constant_prefer_precise_compact
/// @return std::true_type
template <typename char_type, typename T>
concept compiler_constant_precise_compact_preferred =
	::std::integral<char_type> &&
	::fast_io::nothrow_precise_reserve_printable<char_type, T> &&
	requires(T &value) {
		{
			print_compiler_constant_prefer_precise_compact(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
		{
			print_reserve_precise_size(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>,
				value)
		} noexcept -> ::std::same_as<::std::size_t>;
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value));
	};

/// @brief Exposes a complete compiler-constant spelling when it is one provider-owned immutable fragment.
/// @details This optional probe runs on the already-materialized proxy before exact compact output.  A nonzero
///          scatter is the complete spelling and remains valid for the synchronous direct write; a zero length means
///          that the spelling is empty or requires another strategy.  The probe must not materialize characters,
///          mutate the proxy, or invoke its ordinary reserve/static-fragment emitters.  It exists specifically to
///          retain a true rodata pointer for one-slice values without first allocating a conservative descriptor array.
/// @fn print_compiler_constant_single_static_fragment
template <typename char_type, typename T>
concept compiler_constant_single_static_fragment_printable =
	::std::integral<char_type> && requires(T const &value) {
		{
			print_compiler_constant_single_static_fragment(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>,
				value)
		} noexcept -> ::std::same_as<
			::fast_io::basic_io_scatter_t<char_type>>;
		// The probe selects an immutable shortcut and must not hide a deterministic failure behind traditional noexcept.
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_compiler_constant_single_static_fragment(
			::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value));
	};

/// @brief Marks an exact-size producer for which preinitializing the destination is a material cost.
/// @details This is a profitability refinement, not another formatting capability. The precise reserve protocol still
///          proves the exact extent and complete overwrite. The marker says that an ordinary concat strategy should
///          use that protocol in the final result only when the result can create live writable characters without
///          first value-initializing the same range. A run-time sized range is the motivating case: it already pays an
///          element traversal to measure the exact extent, so zero-filling the final string before the second traversal
///          adds a third full traversal and one extra complete destination write. Semantic concat has its own whole-
///          graph cost model and deliberately does not consume this leaf-level marker.
/// @fn         print_precise_resize_initialization_sensitive
/// @return     std::true_type
template <typename char_type, typename T>
concept precise_resize_initialization_sensitive_printable =
	::std::integral<char_type> && precise_reserve_printable<char_type, T> && requires {
		{
			print_precise_resize_initialization_sensitive(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      static_precise_reserve_printable
/// @details    Refines reserve printing with a precise length known as a constant expression without inspecting an
///             object. Because sizing performs no range traversal, composites of statically precise elements do not
///             require a second iterator pass merely to determine their allocation.
/// @fn         print_reserve_static_precise_size
/// @brief      Returns the precise size of the printed object as a constant expression.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @return     ::std::size_t                               the precise size of the output
template <typename char_type, typename T>
concept static_precise_reserve_printable =
	::std::integral<char_type> &&
	(reserve_printable<char_type, T> || dynamic_reserve_printable<char_type, T>) && requires {
		{
			print_reserve_static_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::size_t>;
		typename ::fast_io::details::compile_time_size_constant<print_reserve_static_precise_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
		requires(print_reserve_static_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) != SIZE_MAX);
		// A statically exact output still denotes a C++ contiguous range; an extent outside ptrdiff_t cannot be used by
		// the pointer arithmetic required by exact allocation and semantic coalescing.
		requires(print_reserve_static_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <
				 static_cast<::std::size_t>(PTRDIFF_MAX));
	};

/// @brief      reserve_scatters_printable
/// @details    Describes an object that can be represented by a compile-time-bounded descriptor array plus optional
///             caller-owned reserve storage. The size result is a capacity: define may omit empty components and
///             return earlier end pointers, but it must not advance either pointer beyond the reported capacity.
///             Descriptors may point into the input only when that storage outlives the enclosing print operation.
/// @fn         print_reserve_scatters_size
/// @brief      Returns descriptor and reserve-storage capacities.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @return     ::fast_io::reserve_scatters_size_result     descriptor capacity and reserve-storage capacity
/// @fn         print_reserve_scatters_define
/// @brief      Materializes descriptors and any owned characters into caller-provided storage.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param      ::fast_io::basic_io_scatter_t<char_type>*   a pointer to the beginning of the scatters
/// @param      char_type*                                  a pointer to the beginning of the buffer
/// @param      T                                           the object to be printed
/// @return     ::fast_io::basic_reserve_scatters_define_result<char_type>
///                                                         a pointer to the next scatter after printing
///                                                         and a pointer to the next character after printing
template <typename char_type, typename T>
concept reserve_scatters_printable =
	::std::integral<char_type> && requires(T t, ::fast_io::basic_io_scatter_t<char_type> *scatters, char_type *ptr) {
		{
			print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<reserve_scatters_size_result>;
		{
			print_reserve_scatters_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, scatters, ptr, t)
		} -> ::std::same_as<::fast_io::basic_reserve_scatters_define_result<char_type>>;
		typename ::std::integral_constant<
			::std::size_t,
			print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>).scatters_size>;
		typename ::std::integral_constant<
			::std::size_t,
			print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>).reserve_size>;
		// Static descriptor storage cannot be a zero-length array, and both
		// capacities participate in pointer arithmetic.  These nested requirements
		// follow the two NTTP type requirements that prove constant evaluation.
		// Merely checking the return type admits run-time CPOs that every
		// implementation path must later reject.
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .scatters_size != 0u);
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .scatters_size < static_cast<::std::size_t>(PTRDIFF_MAX));
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .reserve_size < static_cast<::std::size_t>(PTRDIFF_MAX));
		// Element-count bounds do not prove allocation-byte bounds. Both products below are formed by typed stack or
		// dynamic storage strategies, so division-before-multiplication is part of protocol admission rather than a
		// late allocator precondition.
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .scatters_size <=
				 (::std::numeric_limits<::std::size_t>::max)() /
					 sizeof(::fast_io::basic_io_scatter_t<char_type>));
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .reserve_size <=
				 (::std::numeric_limits<::std::size_t>::max)() / sizeof(char_type));
	};

/// @brief Optional native-byte refinement of a static reserve-scatters producer.
/// @details The ordinary protocol writes `basic_io_scatter_t<char_type>` whose lengths are character counts. A byte
///          output backend may avoid typed scratch storage and a descriptor conversion only when the producer exposes
///          this separate CPO and writes genuine `io_scatter_t` objects with byte lengths. The capacity and reserve
///          bounds remain those of `print_reserve_scatters_size`; dispatch validates both returned cursors. This
///          refinement grants no aliasing exception and does not replace the portable typed protocol. For the same
///          argument it must describe the same byte sequence as the typed CPO; only descriptor representation and the
///          length unit may differ.
/// @fn       print_reserve_scatters_bytes_define
/// @return   basic_reserve_scatters_bytes_define_result<char_type>
template <typename char_type, typename T>
concept reserve_scatters_bytes_printable =
	reserve_scatters_printable<char_type, T> &&
	requires(T t, ::fast_io::io_scatter_t *scatters, char_type *reserve) {
		{
			print_reserve_scatters_bytes_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, scatters, reserve, t)
		} -> ::std::same_as<::fast_io::basic_reserve_scatters_bytes_define_result<char_type>>;
		requires(print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
					 .scatters_size <=
				 (::std::numeric_limits<::std::size_t>::max)() / sizeof(::fast_io::io_scatter_t));
	};

/// @brief      dynamic_reserve_scatters_printable
/// @details    Refines dynamic reserve printing with an optional object-dependent scatter plan. Requiring the
///             contiguous dynamic-reserve protocol as the base capability is deliberate: concat and buffered
///             destinations still need a canonical contiguous representation, while direct-scatter streams may choose
///             the alternate plan. The run-time size result is a capacity; define may return fewer descriptors or
///             reserve characters, but never more. The exclusion of `reserve_scatters_printable` makes the static and
///             dynamic customization signatures unambiguous during overload selection.
/// @fn         print_reserve_scatters_size
/// @brief      Returns the run-time descriptor and reserve-storage capacities for one object.
/// @fn         print_reserve_scatters_define
/// @brief      Materializes the run-time scatter plan into caller-provided storage.
template <typename char_type, typename T>
concept dynamic_reserve_scatters_printable =
	::std::integral<char_type> && dynamic_reserve_printable<char_type, T> &&
	(!reserve_scatters_printable<char_type, T>) &&
	requires(T t, ::fast_io::basic_io_scatter_t<char_type> *scatters, char_type *ptr) {
		{
			print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
		} -> ::std::same_as<reserve_scatters_size_result>;
		{
			print_reserve_scatters_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, scatters, ptr, t)
		} -> ::std::same_as<::fast_io::basic_reserve_scatters_define_result<char_type>>;
	};

/// @brief Proves that a reserve-scatters plan may remain live while another producer is invoked.
/// @details The reserve-scatters shape concepts prove capacities and cursor types, but cannot prove where returned
///          descriptors point.  A descriptor may name caller-provided reserve storage, immutable input, or shared
///          scratch overwritten by the next customization call.  Multi-producer plans require this explicit opt-in;
///          unmarked objects remain fully printable through immediate single-object scatter emission or their
///          contiguous reserve fallback. This marker is solely a retention proof: it does not promise purity,
///          repeatability, or equivalent results from a second `print_reserve_scatters_define` invocation.
/// @fn      print_borrowed_reserve_scatters_source
/// @brief   Returns `std::true_type` when every produced descriptor remains valid through the enclosing print
///          operation and later producer invocations cannot invalidate it.
template <typename char_type, typename T>
concept borrowed_reserve_scatters_source =
	::std::integral<char_type> &&
	(reserve_scatters_printable<char_type, T> || dynamic_reserve_scatters_printable<char_type, T>) && requires {
		{
			print_borrowed_reserve_scatters_source(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      printable
/// @details    Makes a type printable
/// @warning    This concept will be soon deprecated.
/// @fn         print_define
/// @brief      Prints the object to a device directly.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param      output                                      the output device
/// @param      T                                           the object to be printed
/// @return     void
template <typename char_type, typename T>
concept printable = ::std::integral<char_type> &&
					requires(::fast_io::details::dummy_buffer_output_stream<char_type> out, T t) {
						{
							print_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, out, t)
						} -> ::std::same_as<void>;
					};

namespace details
{

/// @brief Tests the ordinary `print_define` expression against the destination that will actually receive it.
/// @details The public `printable` concept intentionally retains its historical dummy-buffer probe for source
///          compatibility. That probe establishes only that a customization has the generic printable *shape*: ADL
///          may still select an overload constrained specifically to the dummy stream, or reject an overload that is
///          intentionally specific to a real destination. Dispatch code must therefore use this second, internal
///          proof before calling `print_define`. The named requires-expression parameters are lvalues, exactly like
///          the by-value `outstm` and `t` locals used by the freestanding dispatcher.
template <typename char_type, typename output, typename T>
concept direct_printable_to = ::std::integral<char_type> && requires(output out, T t) {
	{
		print_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, out, t)
	} -> ::std::same_as<void>;
};

} // namespace details

/// @brief      buffered_printable_preferred
/// @details    Marks a dynamic-reserve printable whose `print_define` path is a single-pass producer and is preferred
///             when the destination already owns reusable writable storage. This is an explicit cost property, not a
///             consequence of merely having both protocols: an arbitrary `print_define` may issue many writes, while
///             measure-then-materialize can still be faster. Selection therefore requires both this object marker and
///             a buffered/preferred-stream property; unmarked objects retain the ordinary reserve strategy.
/// @fn         print_buffered_preferred
/// @brief      Returns std::true_type for types that prefer print_define on buffered streams.
template <typename char_type, typename T>
concept buffered_printable_preferred =
	::std::integral<char_type> && dynamic_reserve_printable<char_type, T> && printable<char_type, T> && requires {
		{
			print_buffered_preferred(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      put_area_printable_preferred
/// @details    Marks a single-pass producer which is cheaper only when the destination exposes a reusable contiguous
///             put area. This is intentionally narrower than `buffered_printable_preferred`: an append adapter may
///             satisfy the stream-side buffered preference while still paying nontrivial growth/cursor work for every
///             incremental write. Keeping the two costs independent prevents a source optimization measured on a true
///             obuffer from being generalized to every string adapter. The ordinary dynamic-reserve representation
///             remains canonical for unbuffered output and append-only destinations.
/// @fn         print_put_area_preferred
/// @brief      Returns `std::true_type` for types preferring `print_define` only on an actual put area.
template <typename char_type, typename T>
concept put_area_printable_preferred =
	::std::integral<char_type> && dynamic_reserve_printable<char_type, T> && printable<char_type, T> && requires {
		{
			print_put_area_preferred(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a producer whose direct `print_define` is a cheap, genuinely single-pass representation.
/// @details This source marker is intentionally insufficient by itself. Direct range printing can issue one small
///          destination operation per element; that is desirable for a reusable cursor or an explicitly cheap fake/
///          in-memory boundary, but disastrous when each operation is a real system call. Dispatch therefore pairs
///          this proof with `direct_streaming_preferred_stream` and still gives a native run-time scatter plan
///          priority. Separating the two cost axes prevents source shape from pretending to know destination latency.
/// @fn         print_one_pass_preferred
/// @return     std::true_type
template <typename char_type, typename T>
concept one_pass_printable_preferred =
	::std::integral<char_type> && dynamic_reserve_printable<char_type, T> && printable<char_type, T> && requires {
		{
			print_one_pass_preferred(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a one-pass producer which should grow a freshly constructed concat result directly.
/// @details `one_pass_printable_preferred` only proves that direct emission is cheaper for a destination which has
///          independently advertised cheap streaming.  A fresh string has a different trade-off: growing it may be
///          cheaper for a large state-machine producer, but an initialization-sensitive range can still be faster when
///          measured into one contiguous staging allocation and range-constructed.  This second, deliberately narrower
///          source marker lets concat distinguish those cases instead of inferring a construction policy from the
///          ordinary stream policy.  The concat implementation must additionally prove the exact result adapter's
///          direct CPO before selecting the path.
/// @fn         print_concat_one_pass_preferred
/// @return     std::true_type
template <typename char_type, typename T>
concept concat_one_pass_printable_preferred =
	::std::integral<char_type> && one_pass_printable_preferred<char_type, T> && requires {
		{
			print_concat_one_pass_preferred(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a precise producer whose exact extent is already cached and inexpensive to observe.
/// @details `precise_reserve_printable` proves exactness, but its size query may still traverse the complete source.
///          This stronger, destination-neutral cost proof says that querying the same unchanged normalized object is
///          constant-time, stable, and non-throwing. It does not by itself select concat construction, stream output,
///          or destination growth: every consumer must pair it with the storage and lifetime proofs required by that
///          operation.
/// @fn         print_precise_reserve_size_cached
/// @return     std::true_type
template <typename char_type, typename T>
concept cached_precise_reserve_printable =
	::std::integral<char_type> && precise_reserve_printable<char_type, T> &&
	requires(T &value) {
		{
			print_precise_reserve_size_cached(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
		{
			print_reserve_precise_size(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value)
		} noexcept -> ::std::same_as<::std::size_t>;
		// Cached means repeatable and non-failing through both the unwind and Herbception channels.
		requires FAST_IO_HERBCEPTIONS_NOTHROWS(print_reserve_precise_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>, value));
	};

/// @brief Proves that growing an output destination cannot invalidate a precise producer's source representation.
/// @details A print sink already contains observable state and may reallocate when asked for one larger put area.
///          Exact size alone is therefore insufficient: a producer may read views into that same destination, and
///          reserving first would invalidate them. This explicit source promise states that neither its exact-size
///          query nor its exact writer reads storage owned by an output destination, aliases the supplied destination
///          range, or loses any source lifetime when that destination grows or relocates. The print dispatcher still
///          requires a cached size, a non-throwing pointer-reporting exact writer, and independent destination-side
///          reserve/deferred-commit proofs before it can write into unpublished capacity.
/// @fn         print_precise_reserve_output_growth_independent
/// @return     std::true_type
template <typename char_type, typename T>
concept output_growth_independent_precise_reserve_printable =
	::std::integral<char_type> && cached_precise_reserve_printable<char_type, T> &&
	nothrow_precise_reserve_printable<char_type, T> && requires {
		{
			print_precise_reserve_output_growth_independent(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks an exact producer which should size a freshly constructed concat result before writing it.
/// @details `precise_reserve_printable` establishes the exact-size and contiguous-write protocol, but it deliberately
///          says nothing about cost: determining that size may traverse the complete source.  Fresh-result concat must
///          therefore not infer that measuring is cheaper than using a destination's ordinary growth path.  This
///          narrower marker states that the precise-size query is the preferred construction strategy for this source
///          (typically because the extent is cached or available in constant time).  It changes neither printing to an
///          output stream nor concat-to-an-existing-string.  Concat must still prove that its fresh result can either
///          reserve and publish an exact buffer range or establish one exact live range through its precise-resize CPO.
/// @fn         print_concat_fresh_precise_resize_preferred
/// @return     std::true_type
template <typename char_type, typename T>
concept concat_fresh_precise_resize_printable_preferred =
	::std::integral<char_type> && precise_reserve_printable<char_type, T> && requires {
		{
			print_concat_fresh_precise_resize_preferred(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Marks a direct-print producer which may be redirected through one bounded staging put area.
/// @details The producer promises a single forward traversal and endpoint-independent byte semantics. Core may delay
///          intermediate writes until its bounded window fills, but it must never size by replaying the source. A
///          capacity overflow continues through the same staging stream, and an exceptional exit flushes the prefix
///          produced before the exception. The marker alone selects no storage: dispatch additionally requires an
///          unbuffered destination with an explicit whole-output coalescing threshold.
/// @fn         print_single_pass_staging_safe
/// @return     std::true_type
template <typename char_type, typename T>
concept single_pass_staging_printable =
	::std::integral<char_type> && printable<char_type, T> && requires {
		{
			print_single_pass_staging_safe(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      buffered_printable_preferred_stream
/// @details    Marks append-oriented streams whose growth operations provide reusable destination storage even when
///             the type does not expose the ordinary `obuffer_*` cursor protocol. This stream-side marker is separate
///             from `buffered_printable_preferred`: the former proves that incremental emission is cheap for this
///             destination, while the latter proves that the object has a suitable single-pass producer. Requiring
///             both avoids routing such producers directly to unbuffered files.
/// @fn         print_buffered_preferred_stream
/// @brief      Returns `std::true_type` for streams that prefer eligible single-pass producers.
template <typename char_type, typename T>
concept buffered_printable_preferred_stream = ::std::integral<char_type> && requires {
	{
		print_buffered_preferred_stream(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Marks a destination on which incremental direct emission is cheaper than whole-output materialization.
/// @details The marker is deliberately explicit even for a direct-write stream. Structural write capability proves
///          only that a call is valid; it does not distinguish a compiler barrier, an in-memory recorder, and a kernel
///          transition. The paired source marker proves that the selected `print_define` is single-pass. A stream with
///          native scatter output retains the descriptor plan before this policy is considered.
/// @fn         print_direct_streaming_preferred_stream
/// @return     std::true_type
template <typename char_type, typename T>
concept direct_streaming_preferred_stream = ::std::integral<char_type> && requires {
	{
		print_direct_streaming_preferred_stream(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief      semantic_plain_leaf_coalesce_preferred_stream
/// @details    Marks a stream for which one contiguous semantic record is cheaper than replaying the record's
///             already-normalized ordinary leaves through the generic no-pack scanner. This is an explicit cost
///             preference, not a structural property of buffered output: a stream may expose writable cursors yet
///             make every cursor discovery or commit comparatively expensive. Unmarked streams retain the ordinary
///             leaf scanner, which is preferable for native scatter sinks and structurally inlinable fast_io buffers.
///             Wrappers do not inherit this marker automatically because buffering, decoration, or transcoding changes
///             the destination cost model.
/// @fn         print_semantic_plain_leaf_coalesce_preferred_stream
/// @brief      Returns `std::true_type` when plain semantic leaves should remain in the coalescing dispatcher.
template <typename char_type, typename T>
concept semantic_plain_leaf_coalesce_preferred_stream = ::std::integral<char_type> && requires {
	{
		print_semantic_plain_leaf_coalesce_preferred_stream(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief      semantic_optional_scatter_plan_stream
/// @details    Authorizes the IO-level semantic engine to classify library-owned closed scatter conditions without
///             constructing every active-record type. A closed scatter condition may select null or one of two
///             independently proven stable scatter leaves. This is a semantic proof, not a structural performance
///             hint. A provider returning `std::true_type` proves all of the following for its exact stream type and
///             character type:
///
///             1. after mandatory mutex unwrapping, no `status_print_define` owned by the stream's associated
///                namespaces can acquire a record made exclusively from fast_io's status-transparent scalar,
///                codecvt, scatter, and closed-scatter-selection leaves;
///             2. when a selected condition arm has the scatter category selected by the ordinary control scanner, the
///                stream's native-scatter or declared coalescing operation observes the same ordered nonempty
///                descriptor sequence; when no selected arm has scatter category, dynamic contiguous views preserve
///                the scanner's existing reserve-versus-scatter choice; and
///             3. that run-time category selection followed by the ordinary writer preserves write-CPO precedence,
///                primitive-operation boundaries, exception-visible prefixes, and line ownership.
///
///             The consumer independently proves that every source leaf belongs to fast_io's closed, status-transparent
///             set and that the destination has every output operation needed by the two existing strategies.
///             Consequently this marker
///             cannot suppress a status customization associated with a user argument. Wrappers do not inherit the
///             proof automatically: locking, locale, decoration, transcoding, or buffering changes at least one stated
///             obligation.
/// @fn         print_semantic_optional_scatter_plan_stream
/// @brief      Returns `std::true_type` when the exact stream satisfies the complete provider proof above.
template <typename char_type, typename T>
concept semantic_optional_scatter_plan_stream = ::std::integral<char_type> && requires {
	{
		print_semantic_optional_scatter_plan_stream(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief      semantic_optional_scatter_status_transparent_leaf
/// @details    Marks a normalized leaf whose associated namespaces cannot provide `status_print_define` for any active
///             record composed solely of leaves carrying this proof and a stream carrying
///             `semantic_optional_scatter_plan_stream`. The provider must also prove that the leaf's ordinary
///             reserve/scatter CPO has the same named-lvalue meaning before and after optional run-time classification.
///             When that CPO participates in a retained native-scatter operation, it must not invalidate character
///             storage retained from an earlier admitted leaf; this is the same cross-producer lifetime obligation
///             required by the underlying run-time scatter plan. Its contiguous representation must likewise preserve
///             the ordinary scanner's characters and cursor contract.
///             This marker does not make a condition, pack, or width node transparent, and it does not make the type
///             printable; the consumer checks the exact run-time scatter component protocol independently. A provider
///             that owns a whole-record status customization must not define this marker for the participating type.
/// @fn         print_semantic_optional_scatter_status_transparent_leaf
/// @brief      Returns `std::true_type` when the exact normalized leaf satisfies the provider proof above.
template <typename char_type, typename T>
concept semantic_optional_scatter_status_transparent_leaf = ::std::integral<char_type> && requires {
	{
		print_semantic_optional_scatter_status_transparent_leaf(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves absence of whole-record status ownership for one complete normalized semantic source graph.
/// @details The provider returning true_type promises that every active argument sequence reachable by selecting
///          conditions and performing their ordinary input forwarding has no valid status_print_define for the exact
///          effective output and line policy. This includes the empty sequence and all destination/argument-associated
///          namespaces, not merely the provider's namespace. Source tags preserve their exact named-lvalue types,
///          including const qualification. A later status overload invalidating the promise violates this contract.
///          The consumer checks this proof only after source-record status dispatch and mutex unwrapping. The proof
///          eliminates only the otherwise exhaustive status search: it grants no reserve/scatter/context capability,
///          regrouping, forwarding, lifetime, or output-strategy permission. Those remain independently proved.
///          Without this explicit complete-record promise, exact status ownership is checked for every selection.
template <bool line, typename char_type, typename output, typename... Args>
concept semantic_status_free_record = ::std::integral<char_type> && requires {
	{
		print_semantic_status_free_record(io_reserve_type<char_type, output>, ::std::bool_constant<line>{},
										  ::fast_io::io_type_t<Args &>{}...)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief      semantic_optional_scatter_barrier_plan_stream
/// @details    Extends an exact destination's closed-scatter proof to records containing explicitly marked direct-
///             print barriers. A provider returning `std::true_type` proves all of the following for its exact stream
///             and character type:
///
///             1. the stream's associated namespaces own no `status_print_define` for any active record assembled
///                from status-transparent closed-scatter selections and marked barrier leaves;
///             2. an ordinary direct-print barrier terminates the same scatter/reserve prefix in the generic control
///                dispatcher, so emitting that prefix, invoking the barrier once, and continuing with the suffix
///                preserves write-CPO precedence and primitive-operation boundaries; and
///             3. repeated segment dispatch on the same named observer preserves exception-visible prefixes, line
///                ownership, and all destination state needed by the barrier and the following segment.
///
///             The consumer independently requires `semantic_optional_scatter_plan_stream`, proves the exact direct-
///             only shape of every barrier, and bounds every locally enumerated condition segment. Mutex, buffering,
///             decoration, locale, and transcoding wrappers do not inherit this stronger proof automatically.
/// @fn         print_semantic_optional_scatter_barrier_plan_stream
/// @brief      Returns `std::true_type` when the exact stream satisfies the complete provider proof above.
template <typename char_type, typename T>
concept semantic_optional_scatter_barrier_plan_stream =
	::std::integral<char_type> &&
	::fast_io::semantic_optional_scatter_plan_stream<char_type, T> && requires {
		{
			print_semantic_optional_scatter_barrier_plan_stream(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      semantic_optional_scatter_barrier_leaf
/// @details    Marks one normalized direct-print leaf as a status-transparent control-dispatch barrier. The provider
///             proves that none of the leaf's associated namespaces owns `status_print_define` for an active record
///             composed from this leaf, other marked barriers, closed-scatter selections, and a destination carrying the
///             paired barrier-plan proof. It also proves that invoking the ordinary `print_define` once on the same
///             named leaf and destination after the preceding admitted segment is observationally identical to the
///             generic control dispatcher reaching that leaf: output order, primitive boundaries, exceptions, and
///             source lifetimes are unchanged.
///
///             This marker does not make a type printable and does not authorize scatter conversion. The consumer
///             requires the exact destination-specific direct CPO, rejects every reserve/scatter/context/staged and
///             compiler-constant representation, and therefore proves that the leaf is the same non-coalescible
///             boundary in both executions. A general wrapper never inherits the marker from its referent. The
///             consumer may look through fast_io's own transparent `parameter<T>` transport solely to identify this
///             provider, while checking every actual protocol and the complete-record status lookup on the wrapper.
/// @fn         print_semantic_optional_scatter_barrier_leaf
/// @brief      Returns `std::true_type` when the exact normalized leaf satisfies the provider proof above.
template <typename char_type, typename T>
concept semantic_optional_scatter_barrier_leaf = ::std::integral<char_type> && requires {
	{
		print_semantic_optional_scatter_barrier_leaf(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief      semantic_optional_scatter_context_barrier_leaf
/// @details    Marks one normalized context-print leaf as a status-transparent control-dispatch barrier. A provider
///             returning `std::true_type` proves that its associated namespaces own no `status_print_define` for an
///             active record composed from this leaf, other marked barriers, closed-scatter selections, and a paired
///             destination. It also proves that one ordinary context operation consumes the same source synchronously,
///             retains no stream or source storage after completion, and exposes the same ordered windows and exception
///             prefix when invoked after the preceding admitted segment.
///
///             The consumer independently requires the exact context protocol and rejects every scatter, reserve,
///             transcode, staged, compiler-constant, preferred-direct, and one-pass-direct route which precedes context
///             dispatch for the concrete destination. A later ordinary direct fallback may coexist because it is not
///             selected by `print_control_single`. Wrappers never inherit this proof automatically; the consumer may
///             look through fast_io's transparent `parameter<T>` transport only for provider discovery while checking
///             every actual protocol on the transported source.
/// @fn         print_semantic_optional_scatter_context_barrier_leaf
/// @brief      Returns `std::true_type` when the exact normalized context leaf satisfies the provider proof above.
template <typename char_type, typename T>
concept semantic_optional_scatter_context_barrier_leaf =
	::std::integral<char_type> && requires {
		{
			print_semantic_optional_scatter_context_barrier_leaf(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief      scatter_printable_for
/// @details    Proves the scatter customization for one exact call expression, including its cv/ref category.
///             Strategy code must use this refinement when its body calls the customization through a named object,
///             a const-owned value, or an explicitly forwarded expression. A shape proof for `T&&` is not evidence
///             that the same overload is callable with `T&`: ref-qualified producers may intentionally distinguish
///             those expressions, and admitting a different category defers the failure into the strategy body.
/// @fn         print_scatter_define
/// @brief      Generates a printed scatter.
/// @tparam     char_type       the character type of the returned scatter
/// @tparam     expression_type the exact expression type passed to the customization
/// @param      ::fast_io::io_reserve_type_t<char_type, ::std::remove_cvref_t<T>> tag-invoke
/// @param      expression_type                             the object expression to be printed
/// @return     ::fast_io::basic_io_scatter_t<char_type>    a scatter of the printed object
template <typename char_type, typename expression_type>
concept scatter_printable_for = ::std::integral<char_type> && requires {
	{
		print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<expression_type>>,
							 ::std::declval<expression_type>())
	} -> ::std::same_as<basic_io_scatter_t<char_type>>;
};

/// @brief      scatter_printable
/// @details    Preserves the public compatibility query: an unqualified `T` models the forwarding expression `T&&`,
///             exactly as the historical requires-expression did. Internal concat, range, and print strategies use
///             `scatter_printable_for` instead, because their named parameters need not have this value category.
template <typename char_type, typename T>
concept scatter_printable = scatter_printable_for<char_type, T &&>;

/// @brief      borrowed_scatter_source
/// @details    Marks an alias/forwarding source whose resulting character scatter remains valid until the enclosing
///             print operation completes. A returned `basic_io_scatter_t` proves only shape, not lifetime: a producer
///             could return a pointer into a temporary object destroyed at the end of the forwarding expression.
///             Borrowed scatter plans must therefore require this explicit source-side opt-in before retaining the
///             pointer across sizing, descriptor construction, and the final write. The opt-in also promises a
///             repeatable observation during one logical print: invoking the scatter CPO again for the same unchanged
///             object yields the same length and character sequence. This is required by bounded coalescers that first
///             measure a retained run and then materialize it. A consuming producer, or a producer whose output changes
///             on observation, must remain unmarked even when one returned pointer happens to outlive the call.
/// @fn         print_borrowed_scatter_source
/// @brief      Returns `std::true_type` to opt a source type into the borrowed-lifetime contract.
template <typename char_type, typename T>
concept borrowed_scatter_source = ::std::integral<char_type> && requires {
	{
		print_borrowed_scatter_source(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

template <::std::integral char_type>
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>) noexcept
{
	// A scatter is already a non-owning pointer/length view. Copying the view neither creates nor shortens the
	// lifetime of the referenced characters, so the caller's existing lifetime obligation is preserved.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type
print_eager_materialization_safe(io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>) noexcept
{
	// Observing an already-normalized descriptor has no formatter action beyond reading its stored pointer and length.
	return {};
}

/// @brief Marks a scatter source whose observation is independent of every destination's published cursor state.
/// @details Borrowed/repeatable provenance proves descriptor lifetime and stable bytes, but it deliberately does not
///          make a producer pure: a `noexcept` customization may still inspect an output object's cursor through shared
///          state. A strategy which writes several descriptors and delays `obuffer_set_curr` therefore needs this
///          separate source-side promise. The promise covers aliasing, character forwarding, and scatter production for
///          one unchanged object; none may branch on, retain, or mutate an output stream's put-area state. It does not
///          assert that source and destination character ranges are disjoint. Strategies using this marker must retain
///          the historical physical copy order so ordinary overlap preconditions are neither strengthened nor weakened.
/// @fn       print_scatter_output_state_independent
/// @return   std::true_type
template <typename char_type, typename T>
concept scatter_output_state_independent = ::std::integral<char_type> && requires {
	{
		print_scatter_output_state_independent(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

template <::std::integral char_type>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>) noexcept
{
	// Reading an already-materialized pointer/length pair cannot inspect a destination cursor.
	return {};
}

/// @brief Marks a scatter projection as the complete observable print semantics of its source.
/// @details A type may expose a stable, non-throwing scatter and still provide a source-associated status or direct-
///          print customization whose effects are lost when a range strategy copies only the descriptor. This opt-in
///          promises that, after the ordinary alias and character-forwarding steps, emitting the scatter bytes in
///          source order is observationally equivalent to printing the unchanged source as an element and as the
///          element of a separator/element pair. In particular, no source-associated customization requires a
///          different byte sequence, state transition, lock scope, or exception boundary. The destination must make
///          the corresponding output-side promise separately; this marker alone says nothing about cursor folding.
/// @fn       print_scatter_direct_print_equivalent
/// @return   std::true_type
template <typename char_type, typename T>
concept scatter_direct_print_equivalent = ::std::integral<char_type> && requires {
	{
		print_scatter_direct_print_equivalent(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

template <::std::integral char_type>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>) noexcept
{
	// A scatter descriptor is already the complete printable object; it has no hidden formatting protocol to bypass.
	return {};
}

/// @brief Marks an output whose current put-area cursor publications may be folded across raw in-area copies.
/// @details Ordinary obuffer operations prove only callable cursor access. They do not state that `obuffer_curr/end`
///          are observational, that the writable area remains stable until the next output operation, or that several
///          intermediate `obuffer_set_curr` calls have no effect beyond publishing the final pointer. This explicit
///          stream opt-in supplies exactly those semantic facts. A strategy using it must perform no intervening output
///          operation and must admit only source/iterator work independently proved not to inspect the cursor. The
///          output's own status, locking, and direct-print customizations must also make raw ordered scatter copies
///          equivalent for a source carrying the separate direct-print-equivalence marker; otherwise this marker must
///          not be provided. On a capacity miss the strategy must publish the completed prefix before returning to an
///          ordinary output path.
/// @fn       print_deferred_obuffer_commit_safe
/// @return   std::true_type
template <typename char_type, typename output>
concept deferred_obuffer_commit_safe = ::std::integral<char_type> && requires {
	{
		print_deferred_obuffer_commit_safe(
			io_reserve_type<char_type, ::std::remove_cvref_t<output>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves that a destination permits raw ordered scatter copies into its current put area.
/// @details This is stronger than deferred cursor publication: the destination must have stable externally-owned
/// storage and no growth, flush, lock, or status side effect hidden behind the copy. Only fixed buffer views should
/// advertise it.
template <typename char_type, typename output>
concept direct_obuffer_copy_safe = ::std::integral<char_type> && requires {
	{
		print_direct_obuffer_copy_safe(
			io_reserve_type<char_type, ::std::remove_cvref_t<output>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Marks a put area that may receive one completely preflighted non-throwing bounded run.
/// @details This proof is narrower than general deferred cursor folding. The consumer must first prove the whole run
///          fits, perform no intervening output operation, and publish only the final cursor. A failed preflight may
///          neither write nor publish. Outputs with the stronger deferred-commit contract qualify automatically.
template <typename char_type, typename output>
concept single_pass_bounded_obuffer_materialization_safe =
	::std::integral<char_type> &&
	(::fast_io::deferred_obuffer_commit_safe<char_type, output> || requires {
		{
			print_single_pass_bounded_obuffer_materialization_safe(
				io_reserve_type<char_type, ::std::remove_cvref_t<output>>)
		} -> ::std::same_as<::std::true_type>;
	});

/// @brief Proves that a put area's cursor/end pair has a safe address-distance representation.
/// @details The provider promises that each observed pair is either two null sentinels or two pointers into the same
///          live character array with `current <= end`. At run time a dispatcher may consequently compute remaining
///          capacity from their unsigned addresses; the null pair yields zero and the live pair yields the same element
///          count as pointer subtraction, without performing undefined subtraction on lazy null sentinels. This marker
///          does not authorize writes, cursor folding, or reuse across another output operation.
template <typename char_type, typename output>
concept obuffer_address_distance_safe = ::std::integral<char_type> && requires {
	{
		obuffer_address_distance_safe_define(
			io_reserve_type<char_type, ::std::remove_cvref_t<output>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Marks an output whose put area is safe for the compiler-constant materialization strategy.
/// @details The general deferred-commit marker remains the default proof. An output may instead opt in only to this
///          narrower strategy when writing a fully materialized, statically bounded run into its current put area and
///          publishing the cursor once is equivalent to its ordinary buffered path. This narrower opt-in deliberately
///          does not authorize other deferred-cursor strategies.
/// @fn       print_compiler_constant_obuffer_materialization_safe
/// @return   std::true_type
template <typename char_type, typename output>
concept compiler_constant_obuffer_materialization_safe =
	::std::integral<char_type> &&
	(::fast_io::deferred_obuffer_commit_safe<char_type, output> || requires {
		{
			print_compiler_constant_obuffer_materialization_safe(
				io_reserve_type<char_type, ::std::remove_cvref_t<output>>)
		} -> ::std::same_as<::std::true_type>;
	});

template <::std::integral char_type>
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>) noexcept
{
	// The descriptor object contains the borrowed address rather than owning the characters. Copying or destroying the
	// two-word view cannot change the lifetime of the already-external character range.
	return {};
}

/// @brief Marks borrowed descriptor storage whose lifetime is independent of the producer object's identity.
/// @details Entry normalization may copy an ABI-small trivial lvalue before concat constructs a retained descriptor
///          plan. The ordinary borrowed markers prove that a descriptor produced from the original object survives the
///          enclosing operation; they do not prove that a descriptor pointing into a normalized copy survives that
///          copy's destruction. This stronger opt-in permits by-value decay only when destroying or relocating the
///          producer cannot invalidate any descriptor it returns. Unmarked borrowed producers are still normalized to
///          one `parameter<exact-reference>` value, preserving the compact downstream type set without a dangling
///          self-reference. The promise applies to every descriptor protocol supplied by the type: scatter, typed
///          static or dynamic reserve-scatters, and the optional native-byte reserve-scatters refinement. Entry decay
///          precedes typed/native selection, so proving only one refinement cannot justify copying the producer.
/// @fn       print_copy_stable_borrowed_source
/// @return   std::true_type
template <typename char_type, typename T>
concept copy_stable_borrowed_print_source =
	::std::integral<char_type> &&
	(borrowed_scatter_source<char_type, T> || borrowed_reserve_scatters_source<char_type, T>) &&
	requires {
		{
			print_copy_stable_borrowed_source(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};
namespace details
{

template <typename T>
struct print_forward_scatter_result
{
	inline static constexpr bool is_scatter = false;
};

template <typename char_type>
struct print_forward_scatter_result<::fast_io::basic_io_scatter_t<char_type>>
{
	inline static constexpr bool is_scatter = true;
	using scatter_char_type = char_type;
};

/// @brief Validates source provenance when alias/status forwarding produces a borrowed character descriptor.
/// @details A bare scatter records only an address and a length. Once forwarding returns that descriptor, the
///          strategy layer can no longer distinguish stable source storage from scratch reused by the next producer.
///          Requiring the source-side marker at this boundary prevents that irreversible proof loss. Non-scatter
///          results retain their existing admission, while byte scatters are rejected here because this character-
///          forwarding protocol cannot state their element-lifetime contract with an integral character type.
template <typename result_type, typename source_type>
inline consteval bool print_forward_result_has_borrowed_provenance() noexcept
{
	using result_traits =
		::fast_io::details::print_forward_scatter_result<::std::remove_cvref_t<result_type>>;
	if constexpr (!result_traits::is_scatter)
	{
		return true;
	}
	else if constexpr (::std::integral<typename result_traits::scatter_char_type>)
	{
		return ::fast_io::borrowed_scatter_source<
			typename result_traits::scatter_char_type, source_type>;
	}
	else
	{
		return false;
	}
}

/// @brief Calling-convention family used by the conservative value-transport policy.
/// @details Architecture names are not interchangeable with ABIs. In particular, ARM64EC defines x64 compatibility
///          macros but its non-variadic native helper calls use the Classic Arm64 register convention plus Windows
///          user-defined-result restrictions; PowerPC64 ELFv1, ELFv2, and AIX must be selected independently; and CHERI
///          hybrid/pure-capability targets cannot safely reuse their base ISA's aggregate classification. Unmodelled
///          families deliberately reject aggregate value transport until their platform ABI has an explicit proof here;
///          bounded scalar transport remains available because scalar calling classes do not require recovering an
///          aggregate's unavailable member layout.
enum class abi_small_aggregate_model
{
	microsoft_x64,
	windows_arm64,
	sysv_amd64,
	aapcs64,
	aapcs32,
	riscv_integer,
	loongarch,
	powerpc64_elfv1,
	powerpc64_elfv2,
	s390_integer_equivalent,
	mips_o32,
	mips_n32_n64,
	sparc_v9,
	capability_scalar,
	aggregate_indirect_or_stack,
	unmodelled
};

inline constexpr abi_small_aggregate_model native_abi_small_aggregate_model{
#if defined(__CHERI__) || defined(__CHERI_PURE_CAPABILITY__)
	abi_small_aggregate_model::capability_scalar
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
	abi_small_aggregate_model::windows_arm64
#elif (defined(_WIN32) || defined(__CYGWIN__)) && \
	(defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	abi_small_aggregate_model::microsoft_x64
#elif (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	abi_small_aggregate_model::sysv_amd64
#elif defined(_WIN32) && (defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	abi_small_aggregate_model::windows_arm64
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
	abi_small_aggregate_model::aapcs64
#elif defined(_M_ARM) || (defined(__arm__) && \
	(defined(__ARM_EABI__) || defined(__ARM_PCS) || defined(__APPLE__)))
	abi_small_aggregate_model::aapcs32
#elif defined(__riscv)
	abi_small_aggregate_model::riscv_integer
#elif defined(__loongarch__)
	abi_small_aggregate_model::loongarch
#elif (defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && \
	defined(_CALL_ELF) && _CALL_ELF == 1
	abi_small_aggregate_model::powerpc64_elfv1
#elif (defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && \
	defined(_CALL_ELF) && _CALL_ELF == 2
	abi_small_aggregate_model::powerpc64_elfv2
#elif (defined(__s390__) || defined(__s390x__)) && defined(__ELF__) && !defined(__MVS__)
	abi_small_aggregate_model::s390_integer_equivalent
#elif defined(__s390__) || defined(__s390x__)
	abi_small_aggregate_model::unmodelled
#elif defined(__mips_o32) || \
	(defined(_MIPS_SIM) && defined(_ABIO32) && _MIPS_SIM == _ABIO32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI32) && _MIPS_SIM == _MIPS_SIM_ABI32)
	abi_small_aggregate_model::mips_o32
#elif defined(__mips_n32) || defined(__mips_n64) || \
	(defined(_MIPS_SIM) && defined(_ABIN32) && _MIPS_SIM == _ABIN32) || \
	(defined(_MIPS_SIM) && defined(_ABI64) && _MIPS_SIM == _ABI64) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_NABI32) && _MIPS_SIM == _MIPS_SIM_NABI32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI64) && _MIPS_SIM == _MIPS_SIM_ABI64)
	abi_small_aggregate_model::mips_n32_n64
#elif (defined(__sparc__) || defined(__sparc)) && \
	(defined(__arch64__) || defined(__sparcv9) || defined(__sparcv9__) || defined(__sparc64__))
	abi_small_aggregate_model::sparc_v9
#elif defined(__arm__) || defined(_M_ARM) || defined(__powerpc__) || defined(__ppc__) || \
	defined(_M_PPC) || defined(__mips__) || defined(__mips) || defined(__sparc__) || \
	defined(__sparc) || defined(__i386__) || defined(_M_IX86) || defined(__wasm__) || \
	defined(__wasm32__) || defined(__wasm64__)
	abi_small_aggregate_model::aggregate_indirect_or_stack
#else
	abi_small_aggregate_model::unmodelled
#endif
};

/// @brief Conservative source-level value-copy envelope for the selected ABI family.
/// @details RISC-V and LoongArch specify direct aggregates up to twice XLEN/GRLEN. AAPCS32 can split larger fixed
///          composites across r0-r3 and the stack, but this performance policy intentionally stops at two words.
///          AAPCS64 can carry an HFA/HVA of up to four members in SIMD/FP registers even when it exceeds 16 bytes;
///          C++20 cannot recognize that recursive homogeneous shape, so the generic budget remains two words and an
///          exact target-specific type may recover the exceptional path with `abi_value_transport_force_direct`.
///          PowerPC64 ELFv1/ELFv2, all three standard MIPS ABIs, and SPARC V9 likewise have broader allocation rules;
///          two register words are the common bounded hot-call shape selected here. MIPS is keyed by o32/n32/n64,
///          not ISA width: n32 uses 64-bit argument registers despite its 32-bit pointer model, while an unrecognized
///          MIPS EABI/o64 variant must not inherit n64 merely because `__mips64` is present.
///
///          s390 has an explicit 1/2/4/8-byte integer-equivalent aggregate class. Recognized ARM32 EABI/PCS targets use
///          AAPCS; an old or vendor ARM PCS without those ABI markers remains scalar-only. PowerPC32 and SPARC V8 pass
///          generic aggregates through caller storage, and the basic WebAssembly C ABI only flattens special singleton
///          cases which cannot be recognized from size alone; those models therefore admit scalar values only. CHERI
///          hybrid and pure-capability targets also use a scalar-only class. A compiler recognizes capability fields,
///          but C++20 reflection cannot prove that an arbitrary base-ISA aggregate retains their register class.
inline constexpr ::std::size_t abi_small_trivial_argument_max_size{
#if defined(__CHERI__) || defined(__CHERI_PURE_CAPABILITY__)
#if defined(__SIZEOF_CAPABILITY__)
	static_cast<::std::size_t>(__SIZEOF_CAPABILITY__)
#else
	sizeof(void *)
#endif
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
	16u
#elif (defined(_WIN32) || defined(__CYGWIN__)) && \
	(defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	8u
#elif ((defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && \
	   !(defined(__arm64ec__) || defined(_M_ARM64EC))) || \
	defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
	16u
#elif defined(__riscv_xlen)
	2u * (static_cast<::std::size_t>(__riscv_xlen) / 8u)
#elif defined(__loongarch_grlen)
	2u * (static_cast<::std::size_t>(__loongarch_grlen) / 8u)
#elif (defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && \
	defined(_CALL_ELF) && (_CALL_ELF == 1 || _CALL_ELF == 2)
	16u
#elif (defined(__s390__) || defined(__s390x__)) && defined(__ELF__) && !defined(__MVS__)
	8u
#elif defined(__mips_o32) || \
	(defined(_MIPS_SIM) && defined(_ABIO32) && _MIPS_SIM == _ABIO32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI32) && _MIPS_SIM == _MIPS_SIM_ABI32)
	8u
#elif defined(__mips_n32) || defined(__mips_n64) || \
	(defined(_MIPS_SIM) && defined(_ABIN32) && _MIPS_SIM == _ABIN32) || \
	(defined(_MIPS_SIM) && defined(_ABI64) && _MIPS_SIM == _ABI64) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_NABI32) && _MIPS_SIM == _MIPS_SIM_NABI32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI64) && _MIPS_SIM == _MIPS_SIM_ABI64)
	16u
#elif (defined(__sparc__) || defined(__sparc)) && \
	(defined(__arch64__) || defined(__sparcv9) || defined(__sparcv9__) || defined(__sparc64__))
	16u
#elif defined(_M_ARM) || (defined(__arm__) && \
	(defined(__ARM_EABI__) || defined(__ARM_PCS) || defined(__APPLE__))) || defined(__riscv) || \
	defined(__loongarch__)
	8u
#else
	sizeof(::std::size_t)
#endif
};

/// @brief Applies the model-specific, reflection-free argument-layout envelope.
/// @details This is deliberately a necessary filter rather than an ABI classifier. Exact field classes, homogeneous
///          aggregates, packed-member alignment, capability-bearing fields, and available argument registers remain
///          compiler decisions. In particular, a SysV AMD64 aggregate with an unaligned packed member can satisfy both
///          the size and `alignof(T)` bounds while the psABI assigns it the MEMORY class; adding an outer alignment does
///          not prove that every member is naturally aligned. Returning true therefore authorizes only a bounded
///          source-level copy in a family where direct transport is possible for some such layouts. It does not promise
///          registers, suppress stack arguments or hidden result storage, or override the compiler's final lowering.
///          A type author who knows that a particular layout is indirect should provide
///          `abi_value_transport_force_reference`. The separate function keeps every family testable on one host and
///          prevents a future architecture macro from silently broadening an unrelated ABI model.
inline consteval bool abi_small_trivial_argument_layout_envelope(
	abi_small_aggregate_model model, ::std::size_t object_size, ::std::size_t object_alignment,
	bool is_scalar, ::std::size_t maximum_size, ::std::size_t scalar_alignment) noexcept
{
	using enum abi_small_aggregate_model;
	if (model == microsoft_x64 || model == s390_integer_equivalent)
	{
		return (object_size == 1u || object_size == 2u || object_size == 4u || object_size == 8u) &&
			   object_size <= maximum_size && object_alignment <= object_size;
	}
	if (model == capability_scalar || model == aggregate_indirect_or_stack || model == unmodelled)
	{
		return is_scalar && object_size <= maximum_size && object_alignment <= scalar_alignment;
	}
	return object_size <= maximum_size && object_alignment <= maximum_size;
}

/// @brief Applies the independent aggregate-result envelope for a helper which returns a copied value.
/// @details Argument admission is not evidence for result admission. AAPCS32 may split an eight-byte composite across
///          argument registers and the stack, but returns a composite larger than four bytes through caller-provided
///          storage. MIPS o32 returns every structure through a hidden result address. The Linux s390/s390x ELF ABI does
///          the same for every structure or union even though exact 1/2/4/8-byte aggregates have integer-equivalent
///          argument classes. PowerPC64 ELFv1 likewise returns aggregates of every size through caller storage, whereas
///          ELFv2 defines direct result classes for aggregates of at most two doublewords. Collapsing these rules into
///          the argument envelope silently copies the referent into a caller-owned result slot on exactly the targets
///          whose asymmetry matters. The alternative reference wrapper may itself use an indirect aggregate return when
///          the helper is not inlined; the guarantee here is that it does not duplicate the referent, not that it removes
///          every hidden-result ABI boundary.
///
///          Scalar results reuse the bounded scalar argument class: the asymmetries above concern C++ aggregate values,
///          and rejecting an ordinary pointer or integer would merely replace one direct scalar with a reference wrapper.
///          Unknown and explicitly indirect models still reject aggregates. As with the argument filter, a true result
///          is only a conservative layout admission; the compiler remains the final psABI classifier. Microsoft x64's
///          exact-width result shape and Windows ARM64's ordinary AAPCS64 size shape are represented here, but their
///          additional recursive user-defined-type restrictions are handled by the object policy below because byte
///          layout alone cannot establish them.
inline consteval bool abi_small_trivial_result_layout_envelope(
	abi_small_aggregate_model model, ::std::size_t object_size, ::std::size_t object_alignment,
	bool is_scalar, ::std::size_t maximum_size, ::std::size_t scalar_alignment) noexcept
{
	using enum abi_small_aggregate_model;
	if (is_scalar)
	{
		return ::fast_io::details::abi_small_trivial_argument_layout_envelope(
			model, object_size, object_alignment, true, maximum_size, scalar_alignment);
	}
	if (model == aapcs32)
	{
		return object_size <= 4u && object_size <= maximum_size && object_alignment <= 4u &&
			   object_alignment <= maximum_size;
	}
	if (model == mips_o32 || model == s390_integer_equivalent ||
		model == powerpc64_elfv1)
	{
		return false;
	}
	return ::fast_io::details::abi_small_trivial_argument_layout_envelope(
		model, object_size, object_alignment, false, maximum_size, scalar_alignment);
}

/// @brief Detects a type-author override which forbids the generic by-value ABI strategy.
/// @details SysV eightbyte classes, packed-member alignment, AAPCS homogeneous aggregates, compiler `trivial_abi`
///          extensions, and target-specific vector conventions cannot be recovered from an arbitrary C++20 class.
///          These two ADL markers are therefore escape hatches for the type that owns that missing layout evidence.
///          A reference-only marker wins if both are accidentally supplied; contradictory evidence must never make a
///          value path more permissive.
template <typename value_type>
concept abi_value_transport_forced_reference = requires {
	{
		abi_value_transport_force_reference(::fast_io::io_type_t<value_type>{})
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Detects a type-author contract selecting value transport for this exact type.
/// @details The marker does not alter a compiler calling convention; its author must already know that the native ABI
///          and the exact completed type make the value path appropriate. It overrides only the library's conservative
///          architecture layout envelope. The library still requires trivial copy/move construction and destruction,
///          because an ABI attribute cannot make repeated observable C++ constructor side effects semantically free.
///          Because the marker is shared by the argument and result policies, supplying it is a deliberately strong
///          contract: the type author attests direct transport in both directions. A direction-specific ABI exception
///          should remain on the conservative path until it has its own model rather than weakening this bidirectional
///          promise.
template <typename value_type>
concept abi_value_transport_forced_direct = requires {
	{
		abi_value_transport_force_direct(::fast_io::io_type_t<value_type>{})
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Checks the direction-independent C++ object conditions for optional value transport.
/// @details The C++ ABI first applies non-trivial-for-calls rules, so a small class with a non-trivial copy/move
///          constructor or destructor can still be passed indirectly. The relevant helpers construct from both a named
///          lvalue and an entry rvalue. Requiring those three special members to be trivial mirrors the operations which
///          the optional value path executes without incorrectly rejecting a type merely because its assignment
///          operator is user-defined. Completeness is checked before the traits because trait implementations may issue
///          a hard diagnostic for a forward declaration rather than produce a substitution failure.
template <typename value_type>
inline consteval bool abi_small_trivial_value_language_object() noexcept
{
	using object_type = ::std::remove_cvref_t<value_type>;
	if constexpr (!::std::same_as<value_type, object_type> || !::std::is_object_v<object_type> ||
				  ::std::is_array_v<object_type> || !requires { sizeof(object_type); })
	{
		return false;
	}
	else if constexpr (!::std::is_trivially_copy_constructible_v<object_type> ||
				   !::std::is_trivially_move_constructible_v<object_type> ||
				   !::std::is_trivially_destructible_v<object_type> ||
				   !::std::constructible_from<object_type, object_type &>)
	{
		return false;
	}
	else
	{
		return true;
	}
}

inline constexpr ::std::size_t abi_small_trivial_scalar_alignment{
	::fast_io::details::native_abi_small_aggregate_model ==
			::fast_io::details::abi_small_aggregate_model::capability_scalar
		? ::fast_io::details::abi_small_trivial_argument_max_size
		: alignof(::std::size_t)};

/// @brief Admits the conservative argument-copy policy after the common language proof.
/// @details This predicate is for helpers whose copied object is a by-value parameter. A true result selects a bounded
///          C++ copy but is not proof that the target ABI will allocate that parameter in registers. It must not be
///          reused for a copied return value: several supported psABIs deliberately classify the same aggregate
///          differently in the two directions. Size and alignment are only reflection-free necessary conditions;
///          semantic permission to duplicate a proxy remains an independent concept at each call site.
template <typename value_type>
inline consteval bool abi_small_trivial_argument_object() noexcept
{
	using object_type = ::std::remove_cvref_t<value_type>;
	if constexpr (!::fast_io::details::abi_small_trivial_value_language_object<value_type>())
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_reference<object_type>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_direct<object_type>)
	{
		return true;
	}
	else
	{
		return ::fast_io::details::abi_small_trivial_argument_layout_envelope(
			::fast_io::details::native_abi_small_aggregate_model, sizeof(object_type),
			alignof(object_type), ::std::is_scalar_v<object_type>,
			::fast_io::details::abi_small_trivial_argument_max_size,
			::fast_io::details::abi_small_trivial_scalar_alignment);
	}
}

/// @brief Admits the conservative result policy after the common language proof.
/// @details This is the only policy used when an optional helper can choose between returning a copied object and a
///          reference wrapper. The argument policy is intentionally not a fallback here: doing so would erase the
///          AAPCS32, MIPS o32, Linux s390/s390x, and PowerPC64 ELFv1 result asymmetries documented above.
///
///          Microsoft x64 additionally permits an exact-width user-defined result in RAX only under recursive
///          C++03-POD-like restrictions: no user-defined constructor, destructor, or copy assignment; no non-public or
///          reference members; no bases or virtual functions; and member types satisfying the same rules. Generic C++20
///          traits cannot prove the recursive member condition or even the absence of an empty base. The fail-closed
///          policy therefore rejects every unmarked class result on that ABI. A type author who knows the complete
///          definition can recover the direct path with `abi_value_transport_force_direct`; scalar results remain
///          admitted without an aggregate proof. Argument transport stays independently optimized because Microsoft
///          applies only the exact-width rule on that side.
///
///          Native Windows ARM64 and ARM64EC have the same reflection problem. Their direct user-defined return class
///          requires, among other conditions, a C++14 aggregate, trivial copy assignment and destruction, no bases or
///          virtual functions, and no non-public members. C++20 traits cannot recursively establish that contract for an
///          arbitrary member graph. The `windows_arm64` model therefore keeps the normal AAPCS64 argument envelope but
///          applies the same fail-closed class-result rule and explicit type-author escape hatch.
template <typename value_type>
inline consteval bool abi_small_trivial_result_object_for_model(
	abi_small_aggregate_model model, ::std::size_t maximum_size,
	::std::size_t scalar_alignment) noexcept
{
	using object_type = ::std::remove_cvref_t<value_type>;
	if constexpr (!::fast_io::details::abi_small_trivial_value_language_object<value_type>())
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_reference<object_type>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_direct<object_type>)
	{
		return true;
	}
	else if constexpr (
		!::std::is_scalar_v<object_type>)
	{
		if (model == ::fast_io::details::abi_small_aggregate_model::microsoft_x64 ||
			model == ::fast_io::details::abi_small_aggregate_model::windows_arm64)
		{
			return false;
		}
		return ::fast_io::details::abi_small_trivial_result_layout_envelope(
			model, sizeof(object_type), alignof(object_type), false, maximum_size,
			scalar_alignment);
	}
	else
	{
		return ::fast_io::details::abi_small_trivial_result_layout_envelope(
			model, sizeof(object_type), alignof(object_type), true, maximum_size,
			scalar_alignment);
	}
}

/// @brief Applies the result policy to the compiler-selected native ABI model.
template <typename value_type>
inline consteval bool abi_small_trivial_result_object() noexcept
{
	return ::fast_io::details::abi_small_trivial_result_object_for_model<value_type>(
		::fast_io::details::native_abi_small_aggregate_model,
		::fast_io::details::abi_small_trivial_argument_max_size,
		::fast_io::details::abi_small_trivial_scalar_alignment);
}

/// @brief Print forwarding's bounded ownership-copy budget.
/// @details A non-lvalue result must become owned regardless of whether the compiler lowers the final return directly
///          or through caller storage. This byte limit controls that mandatory copy rescue; optional lvalue copies use
///          the stricter aggregate-result predicate instead.
inline constexpr ::std::size_t print_forward_transport_max_value_size{
	::fast_io::details::abi_small_trivial_argument_max_size};

/// @brief Proves the exact copy from the forwarding helper's named parameter used by the ABI-small branch.
/// @details A forwarding-reference parameter is an lvalue inside its helper even when the original expression was a
///          prvalue. A true lvalue remains an optional transport optimization and therefore uses the conservative native
///          aggregate-result policy, including its move requirement. A prvalue/xvalue is different: it must become owned
///          before the helper returns, and a deleted move leaves copying the named parameter as the only valid
///          construction.
///          That rescue admits one trivially copied, trivially destroyed object inside the ABI-derived byte/alignment
///          budget even on an ABI which ultimately lowers the return through caller storage. The compiler still owns
///          that final lowering; this predicate proves the executed bounded C++ copy, not a universal register return.
///
///          An available non-trivial move is deliberately excluded. Choosing the copy shortcut in that case would
///          suppress observable move-constructor semantics; the ordinary owning branch must execute that move instead.
///          Completeness precedes every standard trait which may require a complete class.
template <typename result_type>
inline consteval bool print_forward_result_copied_from_named_value() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	using named_source_type = ::std::remove_reference_t<result_type> &;
	if constexpr (!::std::is_object_v<result_value_type> || ::std::is_array_v<result_value_type> ||
					 !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else if constexpr (!::std::is_trivially_copy_constructible_v<result_value_type> ||
				   !::std::is_trivially_destructible_v<result_value_type> ||
				   !::std::constructible_from<result_value_type, named_source_type>)
	{
		return false;
	}
	else if constexpr (::std::is_lvalue_reference_v<result_type>)
	{
		// Existing lvalues always have the cheap reference-wrapper alternative. Result admission, rather than argument
		// admission, is therefore required before this helper may return a copied aggregate.
		return ::fast_io::details::abi_small_trivial_result_object<result_value_type>();
	}
	else if constexpr (::std::constructible_from<result_value_type, result_value_type &&> &&
				   !::std::is_trivially_move_constructible_v<result_value_type>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_reference<result_value_type>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::abi_value_transport_forced_direct<result_value_type>)
	{
		return true;
	}
	else
	{
		return sizeof(result_value_type) <= ::fast_io::details::abi_small_trivial_argument_max_size &&
			   alignof(result_value_type) <= ::fast_io::details::abi_small_trivial_argument_max_size;
	}
}

/// @brief Proves that a print-alias result can cross a subsequent normalization boundary.
/// @details A CPO lvalue result can remain an exact-reference `parameter`. An rvalue may use the ABI-small copy from
///          the helper's named parameter or may be forwarded into owned storage. Alias recognition has no character
///          type; the later character-aware forwarder rechecks its actual branch if a borrowed-source marker disables
///          the small copy. Completeness is tested before the owning constructibility trait.
template <typename result_type>
inline consteval bool print_forward_result_transportable() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (::std::is_void_v<result_type>)
	{
		return false;
	}
	else if constexpr (::std::is_lvalue_reference_v<result_type>)
	{
		return true;
	}
	else if constexpr (!::std::is_object_v<result_value_type> ||
					 !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else if constexpr (
		::fast_io::details::print_forward_result_copied_from_named_value<result_type>())
	{
		return true;
	}
	else
	{
		return ::std::constructible_from<result_value_type, result_type>;
	}
}

/// @brief Character-aware result proof used by status forwarding.
/// @details The borrowed-source identity marker can veto the ABI-small copy because descriptors retained from a copy
///          may point into the destroyed copy. After that veto, an rvalue result must be constructible from its exact
///          category. Thus this predicate is identical to the branch later executed by
///          `io_print_forward_transport_for` without evaluating the CPO twice.
template <::std::integral char_type, typename result_type>
inline consteval bool print_forward_result_transportable_for() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (::std::is_void_v<result_type>)
	{
		return false;
	}
	else if constexpr (::std::is_lvalue_reference_v<result_type>)
	{
		return true;
	}
	else if constexpr (!::std::is_object_v<result_value_type> ||
					 !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else
	{
		constexpr bool copied_from_named_value{
			::fast_io::details::print_forward_result_copied_from_named_value<result_type>()};
		if constexpr (copied_from_named_value)
		{
			// The capability/lifetime graph is relevant only after the cheap ABI-copy proof succeeds. This ordering
			// avoids instantiating unrelated print protocols for every large or nontrivial status result.
			constexpr bool borrowed_source_requires_identity{
				((::fast_io::scatter_printable_for<char_type, result_value_type &> &&
				  ::fast_io::borrowed_scatter_source<char_type, result_value_type>) ||
				 ((::fast_io::reserve_scatters_printable<char_type, result_value_type &> ||
				   ::fast_io::dynamic_reserve_scatters_printable<char_type, result_value_type &>) &&
				  ::fast_io::borrowed_reserve_scatters_source<char_type, result_value_type>)) &&
				!::fast_io::copy_stable_borrowed_print_source<char_type, result_value_type>};
			if constexpr (!borrowed_source_requires_identity)
			{
				return true;
			}
		}
		return ::std::constructible_from<result_value_type, result_type>;
	}
}

/// @brief Tests whether the scan-alias fallback can safely transport an ADL result.
/// @details `io_scan_forward` preserves an lvalue result by reference. An alias prvalue or xvalue must instead be
///          materialized into owned storage before that helper returns; otherwise retaining its parameter reference
///          would expose a dangling result to callers that store the normalization result past the full expression.
///          Testing the exact expression type keeps concept admission identical to that direct construction. A stable
///          noncopyable lvalue remains valid, while a nonmovable prvalue or `Proxy&&` is rejected at the CPO boundary
///          instead of causing an unrelated hard error inside scan dispatch.
template <typename result_type>
inline consteval bool scan_alias_result_transportable() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (::std::is_void_v<result_type>)
	{
		return false;
	}
	else if constexpr (::std::is_lvalue_reference_v<result_type>)
	{
		return true;
	}
	else if constexpr (!::std::is_object_v<result_value_type> ||
					 !requires { sizeof(result_value_type); })
	{
		// Constructibility traits are permitted to diagnose an incomplete class. Keep this ordered gate ahead of the
		// exact-category construction proof so a forward-declared prvalue or xvalue CPO result makes alias recognition
		// false by substitution instead of producing a hard standard-library trait diagnostic.
		return false;
	}
	else
	{
		return ::std::constructible_from<result_value_type, result_type>;
	}
}

} // namespace details

/// @brief      alias_scannable
/// @details    Alias scannable enables a type to be scanned using an alias
///             where the scanning function is defined on the alias type.
/// @fn         scan_alias_define
/// @brief      Indicates the alias type.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_alias_t                       tag-invoke
/// @param      T                                           the object to be scanned
/// @return     your_alias_type                             your alias type, commonly owns a reference to T
/// @note       A `void` customization is not an alias representation. A stable lvalue proxy may be noncopyable, while
///             a prvalue or xvalue must be constructible as owned storage because `io_scan_forward` cannot return a
///             reference to its rvalue parameter as a generally safe public result. No common proxy class is imposed.
template <typename T>
concept alias_scannable = requires(T &&t) {
	scan_alias_define(io_alias, ::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::details::scan_alias_result_transportable<decltype(scan_alias_define(io_alias, ::fast_io::freestanding::forward<T>(t)))>();
};

/// @brief      alias_printable
/// @details    Alias printable enables a type to be printed using an alias
///             where the printing function is defined on the alias type.
/// @fn         print_alias_define
/// @brief      Indicates the alias type.
/// @tparam     <auto-inferred>
/// @param      ::fast_io::io_alias_t                       tag-invoke
/// @param      T                                           the object to be printed
/// @return     your_alias_type                             your alias type, commonly owns T
/// @note       A `void` result is not a transport representation. Stable lvalue results may be noncopyable; a prvalue
///             or xvalue must survive the named forwarding parameter as a newly owned value. Rejecting a result which
///             cannot cross that boundary keeps recognition substitution-safe and prevents a later hard diagnostic.
///             All accepted results remain subject to the independent scatter-provenance proof below.
template <typename T>
concept alias_printable = requires(T &&t) {
	print_alias_define(io_alias, ::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::details::print_forward_result_transportable<
		decltype(print_alias_define(
			io_alias, ::fast_io::freestanding::forward<T>(t)))>();
	requires ::fast_io::details::print_forward_result_has_borrowed_provenance<
		decltype(print_alias_define(io_alias, ::fast_io::freestanding::forward<T>(t))), T>();
};

/// @brief Recognizes a character-dependent scan proxy whose result can cross the normalization boundary.
/// @details A status CPO may return a stable noncopyable lvalue reference. Any prvalue or xvalue result instead has to
///          become owned storage before `io_scan_forward` returns. Reusing the scan-alias transport proof rejects void,
///          incomplete, and immovable non-lvalue results during substitution and makes concept selection identical to
///          the branch executed by the forwarding helper.
template <typename char_type, typename T>
concept status_io_scan_forwardable =
	::std::integral<char_type> && requires(T &&t) {
		status_io_scan_forward(io_alias_type<char_type>, ::fast_io::freestanding::forward<T>(t));
		requires ::fast_io::details::scan_alias_result_transportable<
			decltype(status_io_scan_forward(
				io_alias_type<char_type>, ::fast_io::freestanding::forward<T>(t)))>();
	};

/// @brief Recognizes a character-dependent print proxy which can cross normalization without losing provenance.
/// @details Stable lvalue results remain borrowed. A non-lvalue result must be safely materializable under the ABI
///          transport policy, and a result exposing borrowed scatters must independently prove that copying does not
///          change their lifetime or identity. These nested proofs match the forwarding helper's exact source category;
///          defining only a callable CPO is insufficient.
template <typename char_type, typename T>
concept status_io_print_forwardable = ::std::integral<char_type> && requires(T &&t) {
	status_io_print_forward(io_alias_type<char_type>, ::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::details::print_forward_result_transportable_for<
		char_type,
		decltype(status_io_print_forward(
			io_alias_type<char_type>, ::fast_io::freestanding::forward<T>(t)))>();
	requires ::fast_io::details::print_forward_result_has_borrowed_provenance<
		decltype(status_io_print_forward(
			io_alias_type<char_type>, ::fast_io::freestanding::forward<T>(t))),
		T>();
};

struct manip_tag_t
{
};

/// @brief Recognizes the nested marker which prevents an existing manipulator from being wrapped as a plain parameter.
/// @details A provider supplies a member type named `manip_tag`; no particular definition is required. This marker
///          proves classification only. It does not prove copy/move construction, ownership, printability, scannability,
///          or safe transport of an rvalue, so every consumer must establish those properties for its exact expression
///          category before constructing or retaining a manipulator object.
template <typename T>
concept manipulator = requires(T t) { typename T::manip_tag; };

/// @brief Compact transport wrapper used to preserve an existing object's exact reference type.
/// @details Print and scan protocol customizations belong to the wrapped type, not to a specialization of `parameter`.
///          The library adapters deliberately make this wrapper transparent and propagate capability, lifetime, and
///          provenance markers from `T`. Defining competing CPO overloads specifically for `parameter<T>` would break
///          that equivalence and is therefore reserved to the library. This restriction also keeps downstream type
///          graphs compact: a wrapper can be copied without re-running alias discovery on its referent.
template <typename T>
struct parameter
{
	using manip_tag = manip_tag_t;
	T reference;
};

namespace details
{

/// @brief Models the exact expression produced by reading a parameter member from a named mutable wrapper.
/// @details Parameter CPO adapters receive or bind a named mutable wrapper, so its member access is an lvalue. A stored
///          value consequently becomes `T&`, a stored `T const&` remains const, and a reference member never acquires
///          const merely because the wrapper does. Constraining the underlying protocol with this expression type
///          makes overload resolution in the requires-clause identical to the adapter body.
template <typename value_type>
using parameter_mutable_member_reference_t =
	decltype((::std::declval<::fast_io::parameter<value_type> &>().reference));

/// @brief Models the exact expression produced by reading a parameter member from a named const wrapper.
/// @details Staged adapters intentionally accept `parameter` by const reference so an owned formatter is observed as
///          const while an actual reference member preserves its original cv-qualification. This separate alias is
///          necessary evidence: using `remove_cvref_t<value_type>` would admit a mutable-only formatter for a const
///          referent and defer the failure until an `auto` return type instantiated the function body.
template <typename value_type>
using parameter_const_member_reference_t =
	decltype((::std::declval<::fast_io::parameter<value_type> const &>().reference));

#if defined(__HERBCEPTIONS__)
/// @brief Classifies the deterministic effect of an exact-size query delegated through `parameter`.
/// @details `member_reference` is the precise expression type of `wrapper.reference`; keeping it independent from the
///          wrapper's storage type makes mutable and const adapters query the same overload that their bodies invoke.
template <::std::integral char_type, typename value_type, typename member_reference>
inline constexpr bool parameter_precise_size_herbceptions_may_fail = throws((
	print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>,
		::std::declval<member_reference>())));

/// @brief Classifies the deterministic effect of precise emission delegated through `parameter`.
/// @details The iterator and extent are named parameters in the public adapter, hence lvalues in the executed call.
///          Modeling those categories here includes any provider-defined argument conversion in the ABI decision.
template <::std::integral char_type, typename value_type, typename Iter, typename member_reference>
inline constexpr bool parameter_precise_define_herbceptions_may_fail = throws((
	print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>,
		::std::declval<Iter &>(), ::std::declval<::std::size_t &>(),
		::std::declval<member_reference>())));
#endif

} // namespace details

/// @brief Declares the reference-owning `parameter` transport safe to copy at a scan dispatch boundary.
/// @details A `parameter<T&>` copy duplicates only a language reference: both wrappers continue to designate the
///          exact same target object, and no target constructor, assignment, allocation, or validation is performed.
///          The library reserves protocol adapters for `parameter` and those adapters delegate through `reference`,
///          so the wrapper address is not part of their observable scanner state. Owning `parameter<T>` is
///          intentionally excluded: copying an owned scanner could change its identity, lifetime, or mutable state,
///          none of which is proved safe merely by a trivial implementation type.
template <::std::integral char_type, typename value_type>
	requires ::std::is_lvalue_reference_v<value_type>
inline constexpr ::std::true_type
scan_proxy_value_transport_safe(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return {};
}

/// @brief Transparently propagates the staged-print state type through `parameter`.
/// @details `parameter` changes only transport and lifetime semantics.  The prepared state remains a property of the
///          referenced formatter, and substituting a different state here would make prepare/emit disagree about the
///          representation stored in the compact staged array.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr decltype(auto)
print_staged_type(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_staged_type(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

/// @brief Propagates the staged-run profitability threshold through `parameter`.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_staged_width(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_staged_width(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

/// @brief Propagates the staged prepared-array upper bound through `parameter`.
/// @details A transport wrapper cannot add registers or stack capacity. Delegating this policy keeps wrapped and
///          unwrapped instances compatible and prevents a finite child limit from silently becoming unbounded.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_staged_max_count(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_staged_max_count(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

/// @brief Propagates the staged fallback placement policy through `parameter`.
/// @details Transport wrappers cannot change the cost model of the referenced formatter. Delegation also keeps mixed
///          wrapped and unwrapped runs compatible under one compile-time staged-group policy.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr bool
print_staged_fallback_inline(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_staged_fallback_inline(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

/// @brief Tests the referenced value with the underlying staged eligibility CPO.
/// @details The wrapper is never itself a formatting value.  Delegating with the stored reference proves that the
///          eligibility decision and the later prepare/emit operations observe the same object and value category.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr bool print_staged_eligible(
	io_reserve_type_t<char_type, parameter<value_type>>, parameter<value_type> const &wrapper) noexcept
{
	return print_staged_eligible(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, wrapper.reference);
}

/// @brief Prepares the referenced value using its native staged formatter.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr auto print_staged_prepare(
	io_reserve_type_t<char_type, parameter<value_type>>, parameter<value_type> const &wrapper) noexcept
{
	return print_staged_prepare(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, wrapper.reference);
}

/// @brief Emits a prepared `parameter` value without reconstructing or copying its formatter.
/// @details The state is named from the underlying staged protocol rather than from the wrapper specialization.  This
///          keeps the adapter acyclic during concept recognition and proves that a mixed run of wrapped and unwrapped
///          instances has one identical prepared-state type and width.
template <::std::integral char_type, typename value_type>
	requires staged_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr char_type *print_staged_define(
	io_reserve_type_t<char_type, parameter<value_type>>, char_type *iter,
	parameter<value_type> const &wrapper,
	::fast_io::details::staged_printable_state_t<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>> const &state) noexcept
{
	return print_staged_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, iter,
							   wrapper.reference, state);
}

namespace details
{

template <::std::integral char_type, typename value_type>
struct parameter_print_context
{
	// The wrapper embeds the child state because it forwards an lvalue whose lifetime remains with the caller. The
	// outer context owner sees this complete aggregate and applies the same bounded-stack or isolated-dynamic policy.
	// The state type itself is selected from the normalized child tag and is consequently independent of the wrapper's
	// cv-qualification; only the object expression supplied to the state differs between these two overloads.
	using context_type = ::fast_io::details::print_context_state_t<char_type, value_type>;
	context_type state;

	inline constexpr context_print_result<char_type *> print_context_define(parameter<value_type> &para,
																			char_type *begin, char_type *end)
		requires context_printable<
			char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
	{
		return state.print_context_define(para.reference, begin, end);
	}

	inline constexpr context_print_result<char_type *> print_context_define(parameter<value_type> const &para,
																			char_type *begin, char_type *end)
		requires context_printable<
			char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
	{
		// Context state is mutable progress owned by this adapter; const applies only to the formatting value. Keeping
		// those two axes separate lets an incremental read-only formatter advance its state without casting away the
		// caller's const qualification or copying an owned formatter at every refill boundary.
		return state.print_context_define(para.reference, begin, end);
	}
};

} // namespace details

template <::std::integral char_type, typename value_type>
	requires context_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr auto print_context_type(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return io_type_t<::fast_io::details::parameter_print_context<char_type, value_type>>{};
}

template <::std::integral char_type, typename output, typename value_type>
	requires ::fast_io::details::direct_printable_to<
		char_type, output,
		::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr void print_define(io_reserve_type_t<char_type, parameter<value_type>>, output &out,
								   parameter<value_type> &wrapper)
{
	// The enclosing print operation already owns or borrows the normalized observer and parameter object. Trivial
	// copyability is not observer-substitution proof: a trivial proxy may keep its cursor inline, while an owning
	// parameter may expose identity-sensitive state. Borrow both objects here and leave any intentional ABI value
	// boundary to the underlying direct-print CPO itself.
	print_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, out, wrapper.reference);
}

template <::std::integral char_type, typename output, typename value_type>
	requires ::fast_io::details::direct_printable_to<
		char_type, output,
		::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr void print_define(io_reserve_type_t<char_type, parameter<value_type>>, output &out,
								   parameter<value_type> const &wrapper)
{
	// A const wrapper containing a value exposes `T const&`, while a wrapper containing a language reference keeps
	// that reference's original cv-qualification. The dedicated overload therefore preserves both ordinary const
	// semantics and `parameter<T&>` transparency; accepting by value would instead either require a copy or recover
	// mutable access through an adapter-local owner.
	print_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, out, wrapper.reference);
}

template <::std::integral char_type, typename value_type>
	requires reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, parameter<value_type>>)
{
	return print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, parameter<value_type>>,
												  parameter<value_type> &para)
{
	// The strategy engine presents its single normalized owner as a named lvalue. Borrowing it keeps a move-only or
	// identity-sensitive formatter viable and makes the size and define phases observe the same object.
	return print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, parameter<value_type>>,
												  parameter<value_type> const &para)
{
	// Measurement and materialization must observe the same const owner. This is stronger than merely making the call
	// well formed: an identity-sensitive formatter may cache derived state in mutable members, so copying the wrapper
	// between phases would change which object receives those logically-const updates.
	return print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_with_possible_static_stack_size<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_reserve_static_stack_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

template <::std::integral char_type, typename value_type>
	requires context_printable_with_static_buffer_size<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_context_static_buffer_size(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_context_static_buffer_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

template <::std::integral char_type, typename value_type>
	requires(reserve_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>> ||
			 dynamic_reserve_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>)
inline constexpr auto print_reserve_define(io_reserve_type_t<char_type, parameter<value_type>>, char_type *begin,
										   parameter<value_type> &para)
{
	return print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires(reserve_printable<
				 char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>> ||
			 dynamic_reserve_printable<
				 char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>)
inline constexpr auto print_reserve_define(io_reserve_type_t<char_type, parameter<value_type>>, char_type *begin,
										   parameter<value_type> const &para)
{
	// The size-only tag is cv-neutral, but the producer expression is not. Mirror the exact const member expression
	// here so both static- and dynamic-reserve concepts reject a mutable-only child cleanly and accept a read-only one.
	return print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires printable_internal_shift<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr auto print_define_internal_shift(io_reserve_type_t<char_type, parameter<value_type>>,
												  parameter<value_type> &para)
{
	// The underlying CPO takes only the formatter object. The former extra iterator argument made every parameter-
	// wrapped internal-alignment formatter invisible to the two-argument concept and to all width call sites.
	return print_define_internal_shift(
		io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires printable_internal_shift<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr auto print_define_internal_shift(io_reserve_type_t<char_type, parameter<value_type>>,
												  parameter<value_type> const &para)
{
	// Internal placement queries formatting semantics but does not grant mutation rights. Forwarding the const member
	// expression keeps width composition available for read-only formatters without weakening a mutable-only protocol.
	return print_define_internal_shift(
		io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

/// @brief Forwards an exact-size query through a mutable parameter while preserving the delegated exception contract.
template <::std::integral char_type, typename value_type>
	requires precise_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t print_reserve_precise_size(io_reserve_type_t<char_type, parameter<value_type>>,
														  parameter<value_type> &para)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::parameter_precise_size_herbceptions_may_fail<
			char_type, value_type,
			::fast_io::details::parameter_mutable_member_reference_t<value_type>>),
		noexcept(print_reserve_precise_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference)))
{
	// The wrapper has exactly the child's deterministic effect and traditional exception specification. The member is
	// evaluated as the same named lvalue in both this declaration proof and the delegated call below.
	return print_reserve_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

/// @brief Forwards an exact-size query through a const parameter without copying its cache-bearing source.
template <::std::integral char_type, typename value_type>
	requires precise_reserve_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t print_reserve_precise_size(io_reserve_type_t<char_type, parameter<value_type>>,
														  parameter<value_type> const &para)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::parameter_precise_size_herbceptions_may_fail<
			char_type, value_type,
			::fast_io::details::parameter_const_member_reference_t<value_type>>),
		noexcept(print_reserve_precise_size(
			io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference)))
{
	// Exact sizing may update a logically-const cache, so the const member is borrowed from the caller's object. The
	// declaration and body consequently preserve both object identity and the child's complete failure contract.
	return print_reserve_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires static_precise_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_reserve_static_precise_size(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return print_reserve_static_precise_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

/// @brief Forwards mutable precise emission and preserves the underlying writer's endpoint type and noexcept contract.
template <::std::integral char_type, typename value_type, typename Iter>
	requires precise_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr decltype(auto)
print_reserve_precise_define(io_reserve_type_t<char_type, parameter<value_type>>, Iter begin, ::std::size_t n,
							 parameter<value_type> &para)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::parameter_precise_define_herbceptions_may_fail<
			char_type, value_type, Iter,
			::fast_io::details::parameter_mutable_member_reference_t<value_type>>),
		noexcept(print_reserve_precise_define(
			io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, n, para.reference)))
{
	// Preserve the exact endpoint type and value category. The formal effect predicate mirrors the complete named-call
	// expression, including any conversion into a provider-owned by-value parameter before the callee is entered.
	return print_reserve_precise_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, n,
										para.reference);
}

/// @brief Forwards const precise emission without erasing its endpoint or complete-expression exception behavior.
template <::std::integral char_type, typename value_type, typename Iter>
	requires precise_reserve_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr decltype(auto)
print_reserve_precise_define(io_reserve_type_t<char_type, parameter<value_type>>, Iter begin, ::std::size_t n,
							 parameter<value_type> const &para)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::parameter_precise_define_herbceptions_may_fail<
			char_type, value_type, Iter,
			::fast_io::details::parameter_const_member_reference_t<value_type>>),
		noexcept(print_reserve_precise_define(
			io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, n, para.reference)))
{
	// Const transport changes neither the selected child overload nor its complete failure contract. Returning
	// `decltype(auto)` also prevents the transparent wrapper from materializing an endpoint reference result.
	return print_reserve_precise_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, begin, n,
										para.reference);
}

template <::std::integral char_type, typename value_type>
	requires precise_resize_initialization_sensitive_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_precise_resize_initialization_sensitive(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Entry decay changes transport and ownership, not the producer's write count. Forwarding this cost marker makes
	// ordinary concat apply the same destination-initialization gate to a direct value and to its compact parameter.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires scatter_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr auto print_scatter_define(io_reserve_type_t<char_type, parameter<value_type>>,
										   parameter<value_type> &para)
{
	return print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires scatter_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr auto print_scatter_define(io_reserve_type_t<char_type, parameter<value_type>>,
										   parameter<value_type> const &para)
{
	// A const owning wrapper must expose its member as const. Copying the wrapper here would silently recover mutable
	// access and could return a descriptor into an adapter-local object; binding it preserves both cv semantics and
	// storage identity.
	return print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires scatter_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr auto print_scatter_define(io_reserve_type_t<char_type, parameter<value_type>> tag,
										   parameter<value_type> &&para)
{
	// Bind the caller's wrapper rather than materializing an adapter-local copy. A descriptor may point into an owned
	// `para.reference`; the rvalue object remains alive for the caller's full expression, whereas a by-value adapter
	// would destroy its copy before the returned descriptor could even be consumed.
	return print_scatter_define(tag, para);
}

template <::std::integral char_type, typename value_type>
	requires buffered_printable_preferred<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_buffered_preferred(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// `parameter` changes only passing semantics. Forwarding the cost marker keeps dispatch identical to printing the
	// referenced value directly.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires put_area_printable_preferred<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_put_area_preferred(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Entry decay may preserve an identity-sensitive lvalue in `parameter<T&>`. The wrapper does not change the
	// producer's cost model, so the put-area-only marker must follow the exact referenced protocol just like the
	// underlying print CPOs; otherwise safe normalization would accidentally disable the intended fast path.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires one_pass_printable_preferred<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_one_pass_preferred(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Parameter transport preserves the same one-pass `print_define` graph. It can forward an established cost proof,
	// but never creates one for an arbitrary dynamic-reserve producer merely because the wrapper is cheap to copy.
	return {};
}

/// @brief Preserves a wrapped producer's non-consuming bounded-materialization preference.
template <::std::integral char_type, typename value_type>
	requires single_pass_bounded_materialization_source<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// The wrapper changes transport only. Preserve the source author's non-consuming bounded-size contract instead of
	// silently demoting a normalized parameter to replay-based dynamic sizing in print, concat, or another consumer.
	return {};
}

/// @brief Delegates bounded-size discovery to the exact source stored by a const parameter wrapper.
template <::std::integral char_type, typename value_type>
	requires single_pass_bounded_materialization_source<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
[[nodiscard]] inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type, parameter<value_type>>,
	parameter<value_type> const &wrapper, ::std::size_t maximum_size) noexcept
{
	// Query the exact const member expression exposed by a const parameter. The generic accessor preserves the
	// underlying CPO's fail-with-SIZE_MAX contract and prevents this transparent adapter from inventing another bound.
	return ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
		wrapper.reference, maximum_size);
}

/// @brief Preserves the wrapped source's explicit one-pass staging safety proof.
template <::std::integral char_type, typename value_type>
	requires single_pass_staging_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_single_pass_staging_safe(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Parameter owns or references the same producer for the complete synchronous operation. It therefore preserves an
	// existing endpoint-independent one-pass proof, but never manufactures that proof for an arbitrary print_define.
	return {};
}

/// @brief Preserves the wrapped source's cached precise-size cost proof.
template <::std::integral char_type, typename value_type>
	requires cached_precise_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_precise_reserve_size_cached(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// `parameter` changes transport only. Its exact-size adapter delegates to the same member expression and mirrors
	// that complete call's exception specification, so the underlying cached-size proof remains valid.
	return {};
}

/// @brief Preserves source independence from output growth across transparent parameter transport.
template <::std::integral char_type, typename value_type>
	requires output_growth_independent_precise_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_precise_reserve_output_growth_independent(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// A reference wrapper does not introduce character storage, while an owning wrapper retains the source object for
	// the complete synchronous operation. It therefore preserves, but never manufactures, relocation independence.
	return {};
}

/// @brief Forwards the wrapped producer's preference for direct growth of a fresh concat result.
template <::std::integral char_type, typename value_type>
	requires concat_one_pass_printable_preferred<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_concat_one_pass_preferred(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// A parameter changes transport only.  Forward the narrower fresh-result construction preference exactly as the
	// ordinary one-pass stream marker is forwarded; never manufacture it for an arbitrary dynamic producer.
	return {};
}

/// @brief Forwards the wrapped producer's preferred exact fresh-result construction policy.
template <::std::integral char_type, typename value_type>
	requires concat_fresh_precise_resize_printable_preferred<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_concat_fresh_precise_resize_preferred(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Parameter normalization changes transport only.  Preserve the source author's exact fresh-construction cost
	// proof, just as the precise size/define CPOs above preserve the underlying formatter and its cv-qualification.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires borrowed_scatter_source<char_type, ::std::remove_cvref_t<value_type>>
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// A reference specialization preserves the caller's source; an owning specialization keeps the source object inside
	// the wrapper for the entire enclosing operation. Neither form creates separate character scratch, so it preserves
	// (but cannot create) the underlying source's borrowed-lifetime and repeatability guarantee.
	return {};
}

/// @brief Preserves an eager-materialization proof through normalized parameter transport.
/// @details The wrapper changes ownership/category only. It forwards an existing proof for the exact member expression
///          but never grants speculative formatting to an arbitrary lvalue merely because it was wrapped.
template <::std::integral char_type, typename value_type>
	requires eager_materialization_safe_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_eager_materialization_safe(io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	return {};
}

/// @brief Preserves a wrapped scatter source's independence from destination cursor state.
template <::std::integral char_type, typename value_type>
	requires scatter_output_state_independent<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Reading through the wrapper does not introduce access to an output cursor. Forward the exact source proof so a
	// deferred-commit put-area plan treats parameter<T> and its underlying producer identically.
	return {};
}

/// @brief Preserves the wrapped source's explicit equivalence between scatter and direct emission.
template <::std::integral char_type, typename value_type>
	requires scatter_direct_print_equivalent<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// The transparent print adapters delegate every observable operation to `reference`; retaining the source's explicit
	// scatter/direct equivalence proof therefore cannot hide a parameter-specific status or formatting side effect.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires(
		(::std::is_reference_v<value_type> &&
		 (borrowed_scatter_source<
			  char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>> ||
		  borrowed_reserve_scatters_source<
			  char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>)) ||
		(!::std::is_reference_v<value_type> &&
		 copy_stable_borrowed_print_source<
			 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>))
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Copying a reference parameter preserves the same referent, so even a self-borrowing producer remains stable.
	// An owning parameter relocates its producer and therefore forwards only an existing source-independence proof.
	return {};
}

template <::std::integral char_type, typename value_type>
	requires borrowed_reserve_scatters_source<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type
print_borrowed_reserve_scatters_source(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// Parameter transport neither relocates the caller-owned reserve area nor invokes the wrapped producer.  It can
	// therefore preserve an existing retention proof but cannot manufacture one for an unmarked formatter.
	return {};
}

// Scatter capacities and returned end pointers are forwarded as one protocol. In particular, these adapters do not
// reinterpret a capacity as an exact count: an underlying define operation remains free to omit empty descriptors.
template <::std::integral char_type, typename value_type>
	requires reserve_scatters_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr reserve_scatters_size_result
print_reserve_scatters_size(io_reserve_type_t<char_type, parameter<value_type>>)
{
	return print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_scatters_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr reserve_scatters_size_result
print_reserve_scatters_size(io_reserve_type_t<char_type, parameter<value_type>>,
							parameter<value_type> &para)
{
	return print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>,
									   para.reference);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_scatters_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr reserve_scatters_size_result
print_reserve_scatters_size(io_reserve_type_t<char_type, parameter<value_type>>,
							parameter<value_type> const &para)
{
	return print_reserve_scatters_size(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>,
									   para.reference);
}

template <::std::integral char_type, typename value_type>
	requires dynamic_reserve_scatters_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr reserve_scatters_size_result
print_reserve_scatters_size(io_reserve_type_t<char_type, parameter<value_type>> tag,
							parameter<value_type> &&para)
{
	return print_reserve_scatters_size(tag, para);
}

template <::std::integral char_type, typename value_type>
	requires(reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>> ||
			 dynamic_reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>)
inline constexpr basic_reserve_scatters_define_result<char_type>
print_reserve_scatters_define(io_reserve_type_t<char_type, parameter<value_type>>,
							  basic_io_scatter_t<char_type> *scatters, char_type *reserve,
							  parameter<value_type> &para)
{
	return print_reserve_scatters_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, scatters,
										 reserve, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires(reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>> ||
			 dynamic_reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>)
inline constexpr basic_reserve_scatters_define_result<char_type>
print_reserve_scatters_define(io_reserve_type_t<char_type, parameter<value_type>>,
							  basic_io_scatter_t<char_type> *scatters, char_type *reserve,
							  parameter<value_type> const &para)
{
	return print_reserve_scatters_define(io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, scatters,
										 reserve, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires(reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>> ||
			 dynamic_reserve_scatters_printable<
				 char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>)
inline constexpr basic_reserve_scatters_define_result<char_type>
print_reserve_scatters_define(io_reserve_type_t<char_type, parameter<value_type>> tag,
							  basic_io_scatter_t<char_type> *scatters, char_type *reserve,
							  parameter<value_type> &&para)
{
	return print_reserve_scatters_define(tag, scatters, reserve, para);
}

template <::std::integral char_type, typename value_type>
	requires reserve_scatters_bytes_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr basic_reserve_scatters_bytes_define_result<char_type>
print_reserve_scatters_bytes_define(
	io_reserve_type_t<char_type, parameter<value_type>>, io_scatter_t *scatters,
	char_type *reserve, parameter<value_type> &para)
{
	return print_reserve_scatters_bytes_define(
		io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, scatters, reserve,
		para.reference);
}

template <::std::integral char_type, typename value_type>
	requires reserve_scatters_bytes_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr basic_reserve_scatters_bytes_define_result<char_type>
print_reserve_scatters_bytes_define(
	io_reserve_type_t<char_type, parameter<value_type>>, io_scatter_t *scatters,
	char_type *reserve, parameter<value_type> const &para)
{
	return print_reserve_scatters_bytes_define(
		io_reserve_type<char_type, ::std::remove_cvref_t<value_type>>, scatters, reserve,
		para.reference);
}

template <::std::integral char_type, typename value_type>
	requires reserve_scatters_bytes_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr basic_reserve_scatters_bytes_define_result<char_type>
print_reserve_scatters_bytes_define(
	io_reserve_type_t<char_type, parameter<value_type>> tag, io_scatter_t *scatters,
	char_type *reserve, parameter<value_type> &&para)
{
	return print_reserve_scatters_bytes_define(tag, scatters, reserve, para);
}

/// @brief Recognition-only vocabulary for the disabled experimental repeated-extraction scanner API.
/// @details This protocol models a scanner object that repeatedly produces borrowed views; it is not the state object
///          used to complete one ordinary scan target across refills. No active `scan`, `scan_freestanding`,
///          `parse_by_scan`, or `to` dispatcher consumes this concept. The experimental scanner-context subsystem is
///          currently excluded from the public build, so advertising these CPOs must not be expected to make a type
///          ordinarily scannable. Activation requires separate ownership, EOF, and borrowed-view lifetime rules.
template <typename char_type, typename T>
concept iterative_scannable =
	::std::integral<char_type> && requires(T &t, char_type const *buffer_curr, char_type const *buffer_end) {
		{ scan_iterative_init_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t) };
		{
			scan_iterative_next_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t, buffer_curr, buffer_end)
		} -> ::std::same_as<parse_result<char_type const *>>;
		{
			scan_iterative_eof_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)
		} -> ::std::same_as<fast_io::parse_code>;
	};

/// @brief Contiguous companion vocabulary for the disabled experimental repeated-extraction scanner API.
/// @details Like `iterative_scannable`, this concept is recognition-only and deliberately remains outside ordinary
///          scan dispatch until the experimental context/range owner has explicit terminal-buffer and view-lifetime
///          invariants.
template <typename char_type, typename T>
concept iterative_contiguous_scannable =
	::std::integral<char_type> && requires(T &t, char_type const *buffer_curr, char_type const *buffer_end) {
		{
			scan_iterative_contiguous_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t, buffer_curr,
											 buffer_end)
		} -> ::std::same_as<parse_result<char_type const *>>;
	};

/// @brief A fixed-width scan protocol whose complete input extent is known from the scanner type.
/// @details The dispatcher forms fixed arrays, batching offsets, and direct-buffer bounds from the reported extent;
///          therefore a merely run-time `size_t` is not sufficient even though its return type looks correct. The
///          define operation has exactly two supported contracts: `void` proves that all bit patterns of that extent
///          are accepted, while `parse_code` reports validation failure. Encoding those facts in the concept prevents
///          a type from passing recognition and failing later in an unrelated dispatch instantiation. Zero is a useful
///          semantic extent and remains valid, but every positive extent must fit the pointer-difference domain used by
///          direct spans and the byte-size domain used by staging storage; an unrepresentable type-level extent cannot
///          describe a C++ array range or allocation.
template <typename char_type, typename T>
concept precise_reserve_scannable = ::std::integral<char_type> && requires(char_type const *buffer_curr, T &t) {
	{
		scan_precise_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::size_t>;
	typename ::fast_io::details::compile_time_size_constant<
		scan_precise_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)>;
	requires(scan_precise_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <
			 static_cast<::std::size_t>(PTRDIFF_MAX));
	requires(scan_precise_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>) <=
			 (::std::numeric_limits<::std::size_t>::max)() / sizeof(char_type));
	requires(
		::std::same_as<
			decltype(scan_precise_reserve_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, buffer_curr, t)),
			void> ||
		::std::same_as<
			decltype(scan_precise_reserve_define(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>, buffer_curr, t)),
			parse_code>);
};

/// @brief Refines fixed-width scanning with the exact infallible `void` result contract.
/// @details For every bit pattern in the advertised fixed extent, the CPO must assign the target without reporting a
///          parse failure and consume exactly that complete extent. The concept does not assert `noexcept`; consumers
///          which aggregate calls or delay cursor publication must prove non-throwing invocation independently.
template <typename char_type, typename T>
concept precise_reserve_scannable_no_error =
	precise_reserve_scannable<char_type, T> && requires(char_type const *buffer_curr, T &t) {
		{
			scan_precise_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, buffer_curr, t)
		} -> ::std::same_as<void>;
	};

/// @brief Proves that a normalized scan proxy may cross an additional by-value transport boundary.
/// @details Trivial copyability is an object-model property, not a semantic proof: a trivial proxy may still use its
///          own address as identity, contain a self-relative pointer, or expect mutations made by one scanner stage to
///          remain in the original proxy object. Returning `true_type` is the proxy author's assertion that a
///          bitwise-equivalent value denotes the same external scan state and that scanner CPOs do not observe the
///          proxy object's address. Dispatch combines this opt-in with independent triviality, object-size,
///          value-category, and ABI-budget checks. A small pack may be owned by the entry controller; a larger pack
///          remains referenced there and crosses only fixed-size cold-fallback chunks by value. Thus this marker never
///          authorizes an unbounded by-value call: every actual call still satisfies the count and rounded-byte limits.
/// @fn       scan_proxy_value_transport_safe
/// @return   ::std::true_type
template <typename char_type, typename T>
concept value_transport_safe_scan_proxy = ::std::integral<char_type> && requires {
	{
		scan_proxy_value_transport_safe(
			io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves that a no-error precise scanner permits delayed aggregate cursor commit.
/// @details The ordinary scalar schedule advances the input cursor immediately before applying each fixed-width CPO.
///          A batched implementation may instead apply several CPOs and publish their combined cursor once only when
///          every participating scanner makes that reordering unobservable. Returning `true_type` is the type author's
///          semantic assertion that its CPO neither observes the source stream cursor indirectly nor exposes callback
///          effects whose meaning depends on those intermediate commits. `noexcept` is checked separately by dispatch;
///          it is necessary for batching but does not by itself prove this stronger observational equivalence.
/// @fn       scan_precise_reserve_aggregate_commit_safe
/// @return   ::std::true_type
template <typename char_type, typename T>
concept aggregate_commit_safe_precise_reserve_scannable =
	precise_reserve_scannable_no_error<char_type, T> && requires {
		{
			scan_precise_reserve_aggregate_commit_safe(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

namespace manipulators
{
}

namespace mnp = manipulators;

} // namespace fast_io
